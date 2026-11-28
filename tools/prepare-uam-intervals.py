"""Prepare a guarded UAM live-range index in the caller's build directory."""
import argparse
import hashlib
from pathlib import Path


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise ValueError('UAM interval source changed; review the SDK before patching')
    return source.replace(before, after)


def prepare(source, header):
    source = source.replace('\r\n', '\n')
    header = header.replace('\r\n', '\n')
    # A new mutator or a changed destructor/header could bypass invalidation.
    # Require review of the complete implementation and class declaration.
    expected = ('5716f8f93cc6a3113be0c386b7edab41f12dfbe32f149448fabcc4151a8fcac8',
                '52310bcb767ed8b37f3bfda9d1a736b68d28782d4d8909c095f4f3481682b231')
    if tuple(hashlib.sha256(text.encode()).hexdigest() for text in (source, header)) != expected:
        raise ValueError('UAM interval implementation or header changed; review SDK cache invalidation')
    source = replace_once(source, '#include "codegen/nv50_ir_util.h"', '''#include "codegen/nv50_ir_util.h"
#include <unordered_map>
#include <vector>
#include <utility>
#include <algorithm>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <mutex>
#endif''')
    source = replace_once(source, 'namespace nv50_ir {', '''namespace nv50_ir {
namespace {
typedef std::vector<std::pair<int, int> > IntervalIndex;
// Keep the SDK Interval layout unchanged: other archive objects embed it.
std::unordered_map<const Interval *, IntervalIndex> interval_indices;
#ifdef __SWITCH__
Mutex interval_index_mutex;
struct IntervalIndexGuard {
   IntervalIndexGuard() { mutexLock(&interval_index_mutex); }
   ~IntervalIndexGuard() { mutexUnlock(&interval_index_mutex); }
};
#else
std::mutex interval_index_mutex;
typedef std::lock_guard<std::mutex> IntervalIndexGuard;
#endif
struct IntervalIndexLock {
#ifdef __SWITCH__
   IntervalIndexGuard guard;
#else
   IntervalIndexGuard guard;
   IntervalIndexLock() : guard(interval_index_mutex) { }
#endif
};
void invalidate_interval_index(const Interval *interval)
{
   IntervalIndexLock lock;
   if (!interval_indices.empty())
      interval_indices.erase(interval);
}
} // namespace
''')
    for signature in ['Interval::clear()', 'Interval::extend(int a, int b)']:
        before = signature + '\n{'
        source = replace_once(source, before, before + '\n   invalidate_interval_index(this);')
    source = replace_once(source, '   assert(a <= b);', '''   assert(a <= b);
   // Only the final range can overlap when a begins at or beyond its start.
   // Appending and extending it need no scan of all preceding live ranges.
   if (head && a >= tail->bgn) {
      if (a > tail->end) {
         tail->next = new Range(a, b);
         tail = tail->next;
      } else if (b > tail->end) {
         tail->end = b;
      }
      return true;
   }''')
    source = replace_once(source,
        '   for (r = (*nextp); r->next; r = r->next);\n   tail = r;',
        '   // Insertion before an existing node cannot change the tail.\n'
        '   if (!r)\n      tail = *nextp;')
    before = 'void Interval::unify(Interval &that)\n{'
    source = replace_once(source, before, before + '\n   invalidate_interval_index(&that);')
    before = '''bool Interval::overlaps(const Interval &that) const
{
#if 1
   Range *a = this->head;
   Range *b = that.head;

   while (a && b) {
      if (b->bgn < a->end &&
          b->end > a->bgn)
         return true;
      if (a->end <= b->bgn)
         a = a->next;
      else
         b = b->next;
   }
#else
   for (Range *rA = this->head; rA; rA = rA->next)
      for (Range *rB = iv.head; rB; rB = rB->next)
         if (rB->bgn < rA->end &&
             rB->end > rA->bgn)
            return true;
#endif
   return false;
}'''
    after = '''bool Interval::overlaps(const Interval &that) const
{
   if (!head || !that.head)
      return false;
   if (head == tail && that.head == that.tail)
      return that.head->bgn < head->end && that.head->end > head->bgn;
   if (tail->end <= that.head->bgn || that.tail->end <= head->bgn)
      return false;
   // Each interval is sorted and coalesced. Index only after construction,
   // invalidate on every mutation, and retire at clear/destruction. A separate
   // lock allows independent compiler instances to use this cache safely.
   IntervalIndexLock lock;
   IntervalIndex &left = interval_indices[this];
   if (left.empty())
      for (Range *r = head; r; r = r->next)
         left.push_back(std::make_pair(r->bgn, r->end));
   IntervalIndex &right = interval_indices[&that];
   if (right.empty())
      for (Range *r = that.head; r; r = r->next)
         right.push_back(std::make_pair(r->bgn, r->end));
   const IntervalIndex *small = &left, *large = &right;
   if (small->size() > large->size())
      std::swap(small, large);
   IntervalIndex::const_iterator begin = large->begin();
   for (IntervalIndex::const_iterator a = small->begin(); a != small->end(); ++a) {
      begin = std::lower_bound(begin, large->end(), a->first,
         [](const std::pair<int, int> &range, int position) { return range.second <= position; });
      if (begin == large->end())
         return false;
      if (begin->first < a->second)
         return true;
   }
   return false;
}'''
    return replace_once(source, before, after)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = prepare(args.source.read_text(), args.source.with_suffix('.h').read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != result:
        args.output.write_text(result)


if __name__ == '__main__':
    main()

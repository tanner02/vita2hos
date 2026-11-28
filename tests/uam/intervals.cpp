#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <utility>
#include <vector>
#include <thread>
#include UAM_INTERVAL_IMPLEMENTATION

using Ranges = std::vector<std::pair<int,int>>;
static void require(bool ok) { if(!ok) { std::fputs("Interval semantic mismatch\n",stderr);std::abort(); } }
static void add(Ranges &ranges,int a,int b)
{
    ranges.emplace_back(a,b);
    std::sort(ranges.begin(),ranges.end());
    Ranges joined;
    for(auto r:ranges) {
        if(!joined.empty() && r.first<=joined.back().second)
            joined.back().second=std::max(joined.back().second,r.second);
        else joined.push_back(r);
    }
    ranges.swap(joined);
}
static bool overlap(const Ranges &a,const Ranges &b)
{
    for(auto x:a)for(auto y:b)
        if(y.first<x.second && y.second>x.first)return true;
    return false;
}
static void worker(unsigned seed,unsigned count)
{
    std::mt19937 random(seed);
    for(unsigned lifetime=0;lifetime<count;lifetime++) {
        std::unique_ptr<nv50_ir::Interval> iv[12];
        Ranges expected[12];
        for(auto &p:iv)p.reset(new nv50_ir::Interval);
        for(unsigned step=0;step<5000;step++) {
            unsigned x=random()%12,y=random()%12;
            if(x==y)y=(y+1)%12;
            switch(random()%7) {
            case 0:case 1:case 2: {
                int a=int(random()%200)-100,b=a+int(random()%12);
                iv[x]->extend(a,b);add(expected[x],a,b);break;
            }
            case 3:
                iv[x]->clear();expected[x].clear();break;
            case 4:
                iv[x]->insert(*iv[y]);
                for(auto r:expected[y])add(expected[x],r.first,r.second);
                break;
            case 5:
                iv[x]->unify(*iv[y]);
                for(auto r:expected[y])add(expected[x],r.first,r.second);
                expected[y].clear();break;
            case 6:
                iv[x].reset(new nv50_ir::Interval(*iv[y]));expected[x]=expected[y];break;
            }
            for(unsigned i=0;i<12;i++) {
                require(iv[i]->isEmpty()==expected[i].empty());
                for(unsigned j=0;j<12;j++)
                    require(iv[i]->overlaps(*iv[j])==overlap(expected[i],expected[j]));
                int pos=int(random()%240)-120;bool contains=false;
                for(auto r:expected[i])if(r.first<=pos && pos<r.second)contains=true;
                require(iv[i]->contains(pos)==contains);
            }
        }
        for(auto &p:iv)p.reset();
    }
}
int main()
{
    worker(12345,100);
#ifdef UAM_INTERVAL_INDEXED
    require(nv50_ir::interval_indices.empty());
#endif
    std::thread a(worker,1,10),b(worker,2,10),c(worker,3,10),d(worker,4,10);
    a.join();b.join();c.join();d.join();
#ifdef UAM_INTERVAL_INDEXED
    require(nv50_ir::interval_indices.empty());
#endif
    std::puts("PASS 700000 interval mutations, independent semantics, copy/unify/clear and four concurrent workers");
#ifdef UAM_INTERVAL_INDEXED
    std::puts("PASS complete interval cache retirement");
#endif
}

#include<bits/stdc++.h>
#include<cstdio>
using namespace std;
const int N = 1000010;
#define lson (o<<1)
#define rson (o<<1|1)
#define fi first
#define sc second
#define dbg(x) cerr<<#x<<" = "<<(x)<<endl;
typedef long long ll;
typedef unsigned int uint;
typedef unsigned long long ull;
const double pi=acos(-1);
const double eps=1e-6;
inline int lowbit(int x){return x&(-x);}
inline int read(){
	int f=1,x=0;char ch;
	do{ch=getchar();if(ch=='-')f=-1;}while(ch<'0'||ch>'9');
	do{x=x*10+ch-'0';ch=getchar();}while(ch>='0'&&ch<='9');
	return f*x;
}
template<typename T> inline T max(T x,T y,T z){return max(max(x,y),z);}
template<typename T> inline T min(T x,T y,T z){return min(min(x,y),z);}
template<typename T> inline T sqr(T x){return x*x;}
template<typename T> inline void checkmax(T &x,T y){x=max(x,y);}
template<typename T> inline void checkmin(T &x,T y){x=min(x,y);}
template<typename T> inline void read(T &x){
x=0;T f=1;char ch;do{ch=getchar();if(ch=='-')f=-1;}while(ch<'0'||ch>'9');
do x=x*10+ch-'0',ch=getchar();while(ch<='9'&&ch>='0');x*=f;
}
template<typename A,typename B,typename C> inline A fpow(A x,B p,C yql){
	A ans=1;
	for(;p;p>>=1,x=1LL*x*x%yql)if(p&1)ans=1LL*x*ans%yql;
	return ans;
}
struct FastIO{
	static const int S=1310720;
	int wpos;char wbuf[S];
	FastIO():wpos(0) {}
	inline int xchar(){
		static char buf[S];
		static int len=0,pos=0;
		if(pos==len)pos=0,len=fread(buf,1,S,stdin);
		if(pos==len)return -1;
		return buf[pos++];
	}
	inline int read(){
		int c=xchar(),x=0;
		while(c<=32&&~c)c=xchar();
		if(c==-1)return -1;
		for(;'0'<=c&&c<='9';c=xchar())x=x*10+c-'0';
		return x;
	}
}io;

inline int get(int x){
    ll ans;
    ans=1LL*rand()*rand();
    return ans%x;
}

map<string,int> mps;
set<string> vm;
set<string> server;
set<int> ID;

vector<string> VM_Vector;


int main(int argc,char *argv[]){
    int n=100,m=100,limit=1e9,T=1000,limit_per_day=1000;
    //scanf("%d%d%d%d%d",&n,&m,&limit,&T,&limit_per_day);
    freopen("../data/gen-test.txt","w",stdout);
    srand(time(NULL));
    printf("%d\n",n);
    int maxcore=0;
    int maxmemory=0;
    for(int i=1;i<=n;i++){
        char name[100]="yyhtql";
        int len=get(5)+4;
        //dbg(len);
        for(int j=1;j<=len;j++)name[5+j]=get(26)+'a';
        name[5+len+1]=0;
        string s;
        s=name;
        server.insert(s);
        //dbg(name);
        int core=get(1024);
        int memory=get(1024);
        if(core&1)core++;
        if(memory&1)memory++;
        checkmax(maxcore,core);
        checkmax(maxmemory,memory);
        int price_deploy=get(500000);
        int preice_day=get(5000);
        printf("(%s, %d, %d, %d, %d)\n",name,core,memory,price_deploy,preice_day);
    }
    printf("%d\n",m);
    for(int i=1;i<=m;i++){
        char name[100]="orzyyh";
        int len=get(5)+4;
        for(int j=1;j<=len;j++)name[j+5]=get(26)+'a';
        int deploy_method=rand()&1;
        int core=get(maxcore/3);
        int memory=get(maxmemory/3);
        string s=name;
        VM_Vector.push_back(s);
        vm.insert(s);
        if(deploy_method){
            if(core&1)core--;
            if(core==0)core=2;
            if(memory&1)memory--;
            if(memory==0)memory=4;
        }
        printf("(%s, %d, %d, %d)\n",name,core,memory,deploy_method);
    }
    printf("%d\n",T);
    while(T--){
        int R;
        do{R=get(limit_per_day)+2;}while(R>limit);
        limit-=R;
        printf("%d\n",R);
        int lastsize=ID.size();
        set<int> TempIDSet;
        while(R--){
            int opt=rand()&1;
            if(lastsize==0&&opt==0)opt=1;
            if(ID.size()==0)opt=1;
            if(opt==1){
                int x=get(VM_Vector.size());
                string name=VM_Vector[x];
                int id;
                do{id=get(100000)+12;}while(ID.count(id)||TempIDSet.count(id));
                TempIDSet.insert(id);
                cout<<"(add, "<<name<<", "<<id<<")"<<endl;
            }
            else{
                set<int>::const_iterator it(ID.begin());
                advance(it,get(ID.size()));
                int id=*it;
                ID.erase(id);
                printf("(del, %d)\n",id);
            }
        }
        for(set<int>::iterator it=TempIDSet.begin();it!=TempIDSet.end();it++)ID.insert(*it);

    }
}
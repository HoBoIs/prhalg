#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class SorterClass {
  int N, threads;
  std::vector<int> *data;
  struct swapper {
    int idx1, idx2;
  };

public:
  struct Pool;
  class simpleQueue {
    int processed_until = 0;

  public:
    struct node {
      static constexpr int size = 1021;
      std::array<swapper, size> data;
      std::atomic_int from = 0;
      int to = 0;
      node *next = 0;
      node *prev = 0;
      struct ite {
        node *n;
        int p;
        swapper &operator*() {
          if (p >= n->to) {
            std::cerr << p << " >= " << n->to << "\n";
          }
          assert(p < n->to);
          assert(p < size);
          return n->data[p];
        }

        ite operator++() {
          p++;
          if (p == n->to) {
            n = n->next;
            p = 0;
          }
          return *this;
        }
        bool operator==(const ite &o) const = default;
      };
      void clear(Pool *s) {
        node *nxt = next;
        if (nxt)
          nxt->clear(s);
        delNode(this, s);
      };
      ite begin() { return {this, from}; }
      ite end() { return {nullptr, 0}; }
    };
    node *head;
    node *tail;
    int len = 0;
    std::mutex m{};
    Pool *sc;
    void init(Pool *s) {
      sc = s;
      head = tail = getNode(sc);
    }
    int size_esstimate() { return len * node::size + tail->to - head->from; }
    void push(const swapper &s) {
      assert(tail->to < tail->size);
      tail->data[tail->to] = s;
      m.lock();
      tail->to++;
      if (tail->to == tail->size) {
        auto newTail = getNode(sc);
        tail->next = newTail;
        newTail->prev = tail;
        ++len;
        tail = newTail;
        assert(tail->to == 0);
      }
      m.unlock();
    }
    node *getContent() {
      if (m.try_lock()) {
        if (head == tail && tail->from == tail->to) {
          m.unlock();
          return nullptr;
        }
        len = 0;
        node *res;
        if (head == tail) {
          res = getNode(sc);
          res->from = head->from.exchange(tail->to);
          res->to = head->from;
          for (int i = res->from; i < res->to; i++) {
            res->data[i] = tail->data[i];
          }
        } else {
          res = head;
          assert(tail->prev);
          tail->prev->next = nullptr;
          tail->prev = nullptr;
          head = tail;
        }
        m.unlock();
        return res;
      } else {
        return nullptr;
      }
    }
    static node *getNode(Pool *s) {
      s->nodepoolLock.lock();
      if (s->nodepool.size()) { // tryLock?
        node *res = s->nodepool.back();
        s->nodepool.pop_back();
        s->nodepoolLock.unlock();
        res->next = res->prev = nullptr;
        res->from = res->to = 0;
        return res;
      } else {
        s->nodepoolLock.unlock();
        return new node;
      }
    }
    static void delNode(node *n, Pool *s) {
      s->nodepoolLock.lock();
      s->nodepool.push_back(n);
      s->nodepoolLock.unlock();
    }
  };
  struct Pool {
    std::vector<simpleQueue::node *> nodepool;
    std::mutex nodepoolLock;
  };

private:
  class jobQueue {
    int jobCnt;
    std::vector<simpleQueue> tasks;
    std::atomic_int finish_cnt;

  public:
    void clear(int N) {
      tasks.clear();
      finish_cnt = N;
    }
    jobQueue(Pool *s, int j = 8) : jobCnt(j), tasks(j) {
      for (auto &x : tasks)
        x.init(s);
    }
    void addTask(const swapper &s, int threadID) { tasks[threadID].push(s); }
    void notify_fin() { --finish_cnt; }
    simpleQueue::node *getTasks(int ID, int banned = -1) {
      if (0 == finish_cnt)
        return nullptr;
      int b = 0;
      if (0 == banned)
        b = 1;
      for (int i = 1; i < jobCnt; ++i)
        if (tasks[i].size_esstimate() > tasks[b].size_esstimate() &&
            banned != i)
          b = i;
      simpleQueue::node *res = tasks[b].getContent();
      if (!res) {
        return getTasks(ID, b);
      }
      return res;
    }
  };
  void processLevelInc(int idx, int ID) {
    if (processed_level[idx] + 1 != layers.size()) {
      auto s = layers[processed_level[idx] + 1].getSwapper(idx);
      int x = s.idx1;
      int y = s.idx2;
      assert(idx == x);
      std::scoped_lock L(processed_level_locks[x], processed_level_locks[y]);
      processed_level[idx]++;
      if (processed_level[x] == processed_level[y]) {
        assert(layers[processed_level[x]].getSwapper(x).idx2 == s.idx2);
        assert(layers[processed_level[y]].getSwapper(y).idx1 == s.idx2);
        jq.addTask(s, ID);
      }
    } else {
      jq.notify_fin();
    }
  }

public:
  Pool pool;
  void executeSwap(const swapper &s, int ID) {
    assert(s.idx1 != s.idx2);

    if (layers[processed_level[s.idx1]].getPair(s.idx1) != s.idx2) {
      std::cerr << s.idx1 << " - " << s.idx2 << "\n";
      std::cerr << processed_level[s.idx1] << " - " << processed_level[s.idx2]
                << "\n";
      std::cerr << layers[processed_level[s.idx1]].getPair(s.idx1) << " !!! ";
      std::cerr << layers[processed_level[s.idx2]].getPair(s.idx2) << "\n";
      std::cerr << layers[processed_level[s.idx1] - 1].getPair(s.idx1)
                << " !!! ";
      std::cerr << layers[processed_level[s.idx2] - 1].getPair(s.idx2) << "\n";
      assert(layers[processed_level[s.idx1]].getPair(s.idx1) == s.idx2);
    }
    if ((s.idx1 < s.idx2) != ((*data)[s.idx1] < (*data)[s.idx2]))
      std::swap((*data)[s.idx1], (*data)[s.idx2]);
    processLevelInc(s.idx1, ID);
    processLevelInc(s.idx2, ID);
  }

private:
  jobQueue jq;
  std::vector<int> processed_level;
  std::vector<std::mutex> processed_level_locks;
  void constructBitonic(int layers_hint) {
    for (int s = 2; s <= N; s *= 2) {
      layers.push_back({s, 1});
      for (int s2 = s / 2; s2 > 1; s2 /= 2) {
        layers.push_back({s2, 0});
      }
    }
  }
  struct thr {
    const int ID;
    jobQueue &jq;
    SorterClass &sorter;
    simpleQueue::node *jobs;
    thr(int id, jobQueue &_jq, SorterClass &_s) : ID(id), jq(_jq), sorter(_s) {}
    void operator()() {
      jobs = jq.getTasks(ID);
      while (jobs) {
        for (auto &j : *jobs) {
          sorter.executeSwap(j, ID);
        }
        jobs->clear(&sorter.pool);
        jobs = jq.getTasks(ID);
      }
    }
  };
  struct layer {
    int size;
    int mode;
    int getPair(int x) {
      int d = x / size;
      x %= size;
      if (mode == 0) {
        return (size / 2 + x) % size + d * size;
      } else {
        return size - x - 1 + d * size;
      }
    }
    swapper getSwapper(int x) { return {x, getPair(x)}; }
  };
  std::vector<layer> layers;

public:
  SorterClass(int _N, int _threads)
      : N(_N), threads(_threads), pool(), jq(&pool, _threads),
        processed_level_locks(_N) {
    constructBitonic(0);
    processed_level.resize(N, 0);
  }
  void sortData(std::vector<int> &dat) {
    data = &dat;
    jq.clear(dat.size());
    for (int i = 0; i < N / 2; i++) {
      jq.addTask(layers[0].getSwapper(i * 2), i % threads);
    }
    std::vector<std::jthread> ths;
    for (int i = 0; i < threads; i++) {
      ths.push_back(std::jthread(thr(i, jq, *this)));
    }
  }
  ~SorterClass() {
    for (auto *x : pool.nodepool)
      delete x;
  }
};

int main() {
  int n;
  int t;
  std::cin>>t>>n;
  int N=1<<n;
  SorterClass s(N, t);
  std::vector<int> v{};
  for (int i = 0; i < N; i++)
    v.push_back(rand());
  /*for (int i = 0; i < 16; ++i) {
    std::cout << v[i] << "\n";
  }*/
  s.sortData(v);
  /*for (int i = 0; i < 16; ++i) {
    std::cout << v[i] << "\n";
  }*/
  int mn = v[0];
  for (int val : v) {
    if (val < mn)
      std::cerr << "PANIC";
    mn = val;
  }
}

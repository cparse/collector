#include <iostream>
#include <memory>

#include "../catch.hpp"

#define DEBUG
#include "./collector.h"

namespace test {

using namespace collector;

struct Data_t : public Collector<Data_t>::CollectableData {
  uint64_t id = 0;
  std::string name = "test";

  Data_t() {}
  Data_t(uint64_t id, std::string name) : id(id), name(name) {}

  // Necessary for the collector to work:
  void collector_access(Collector<Data_t>::callback_t c) {
    // Since there are no possible Data_t contained in here
    // this function does not have to do anything:
    return;
  }
};

struct Data : public Collector<Data_t>::Collectable {
  Data() : Collector<Data_t>::Collectable() {}
  Data(Data_t d) : Collector<Data_t>::Collectable(d) {}
  uint64_t id() { return data->id; }
  std::string name() { return data->name; }
};

// Counts how many items are stored in memory:
template <typename T>
uint64_t count_mem() {
  uint64_t count = 0;
  for (const auto& item : Collector<T>::Collectable::collector.memory) {
    if (item.lock()) {
      ++count;
    }
  }

  for (const auto& item : Collector<T>::Collectable::collector.root_list) {
    if (item) {
      ++count;
    }
  }
  return count;
}

TEST_CASE("Collector Startup", "[collector]") {
  typedef Collector<Data_t> collector_t;
  REQUIRE(collector_t::curr_mark == 0);

  collector_t collector;
  REQUIRE(collector.memory.size() == 0);
  REQUIRE(collector.root_list.size() == 0);
}

/* * * * * Function Level Unit Tests * * * * */

TEST_CASE("Test add function", "[add]") {
  static Collector<Data_t>& collector = Collector<Data_t>::Collectable::collector;

  {
    std::shared_ptr<Data_t> s1;

    // Empty add uses default constructor:
    REQUIRE_NOTHROW(s1 = collector.add());
    REQUIRE(s1->id == 0);
    REQUIRE(s1->name == "test");

    REQUIRE(count_mem<Data_t>() == 1);

    std::shared_ptr<Data_t> s2;

    // Non-empty add uses the given argument:
    REQUIRE_NOTHROW(s2 = collector.add({10, "test1"}));
    REQUIRE(s2->id == 10);
    REQUIRE(s2->name == "test1");

    REQUIRE(count_mem<Data_t>() == 2);
  }

  // When there are no more references to any of them
  // they should be dellocated by reference counting:
  REQUIRE(count_mem<Data_t>() == 0);
}

TEST_CASE("Test root_add function", "[add_root]") {
  static Collector<Data_t>& collector = Collector<Data_t>::Collectable::collector;

  {
    std::shared_ptr<Data_t> s1;

    // Empty add uses default constructor:
    REQUIRE_NOTHROW(s1 = collector.add_root());
    REQUIRE(s1->id == 0);
    REQUIRE(s1->name == "test");

    REQUIRE(count_mem<Data_t>() == 1);

    std::shared_ptr<Data_t> s2;

    // Non-empty add uses the given argument:
    REQUIRE_NOTHROW(s2 = collector.add_root({10, "test1"}));
    REQUIRE(s2->id == 10);
    REQUIRE(s2->name == "test1");

    REQUIRE(count_mem<Data_t>() == 2);
  }

  // When there are no more references to any of them
  // they should still have references inside the root_list
  // and should not be dellocated yet:
  REQUIRE(count_mem<Data_t>() == 2);

  collector.root_list.clear();
}

struct Test_t : public Collector<Test_t>::CollectableData {
  uint64_t ca_count = 0;
  uint64_t id = 0;
  std::string name = "test";

  Test_t() {}
  Test_t(uint64_t id, std::string name) : id(id), name(name) {}

  // Necessary for the collector to work:
  void collector_access(Collector<Test_t>::callback_t c) {
    ++ca_count;
    // Since there are no possible Test_t contained in here
    // this function does not have to do anything:
    return;
  }
};

TEST_CASE("Test mark_callback function", "[mark_callback]") {
  typedef Collector<Test_t> collector_t;
  typedef Collector<Test_t>::Ref_t Ref_t;

  static collector_t& collector = collector_t::Collectable::collector;

  Ref_t t1;

  // Should not throw with empty references:
  REQUIRE_NOTHROW(collector_t::mark_callback(&t1));

  t1 = Ref_t(new Test_t());
  Collector<any_type>::curr_mark = 1;
  t1->collector_mark = 1;

  // Should not run the collector_access
  // if curr_mark == collector_mark:
  REQUIRE_NOTHROW(collector_t::mark_callback(&t1));
  REQUIRE(t1->ca_count == 0);

  Collector<any_type>::curr_mark = 1;
  t1->collector_mark = 0;

  // Should update the mark to curr_mark
  // when not done already and run the collector_access:
  REQUIRE_NOTHROW(collector_t::mark_callback(&t1));
  REQUIRE(t1->collector_mark == 1);
  REQUIRE(t1->ca_count == 1);
}

TEST_CASE("Test reset_callback function", "[reset_callback]") {
  typedef Collector<Test_t> collector_t;
  typedef Collector<Test_t>::Ref_t Ref_t;

  static collector_t& collector = collector_t::Collectable::collector;

  Ref_t t1;

  // Should not throw with empty references:
  REQUIRE_NOTHROW(collector_t::reset_callback(&t1));

  t1 = Ref_t(new Test_t());

  // Should reset non-empty references:
  REQUIRE_NOTHROW(collector_t::reset_callback(&t1));
  REQUIRE(t1.get() == 0);
}

TEST_CASE("Test mark() method", "[mark]") {
  typedef Collector<Test_t> collector_t;

  static collector_t& collector = collector_t::Collectable::collector;

  collector.add_root();
  collector.add_root();
  collector.add_root();

  // Should call each collector_access once:
  REQUIRE_NOTHROW(collector.mark());
  for (const auto& ref : collector.root_list) {
    REQUIRE(ref->ca_count == 1);
  }
}

TEST_CASE("Test sweep() method", "[sweep]") {
  typedef Collector<Test_t> collector_t;
  typedef Collector<Test_t>::Ref_t Ref_t;

  static collector_t& collector = collector_t::Collectable::collector;

  Ref_t r1 = collector.add();
  r1.reset();

  // Should not throw on empty references:
  REQUIRE_NOTHROW(collector.sweep());
  REQUIRE(collector.memory[0].lock().get() == 0);

  Ref_t r2 = collector.add();
  Ref_t r3 = collector.add();

  r2->collector_mark = 2;
  r3->collector_mark = 3;
  Collector<any_type>::curr_mark = 3;
  
  // Should only try to reset if curr_mark != collector_mark
  REQUIRE_NOTHROW(collector.sweep());
  REQUIRE(r2->ca_count == 1);
  REQUIRE(r3->ca_count == 0);
}

TEST_CASE("Test mark_and_sweep() method", "[mark_and_sweep]") {
  typedef Collector<Test_t> collector_t;
  typedef Collector<Test_t>::Ref_t Ref_t;

  static collector_t& collector = collector_t::Collectable::collector;

  // Should increment the curr_mark counter:
  Collector<any_type>::curr_mark = 0;
  REQUIRE_NOTHROW(collector.mark_and_sweep());
  REQUIRE(Collector<any_type>::curr_mark == 1);

  Ref_t r1 = collector.add();
  Collector<any_type>::curr_mark = 3;

  // Should not mark items not linked from root:
  REQUIRE_NOTHROW(collector.mark_and_sweep());
  REQUIRE(r1->ca_count == 1);
  REQUIRE(r1->collector_mark == 0);

  Ref_t r2 = collector.add_root();
  Collector<any_type>::curr_mark = 41;

  // Should try to mark all items from root_list:
  REQUIRE_NOTHROW(collector.mark_and_sweep());
  REQUIRE(r2->ca_count == 1);
  REQUIRE(r2->collector_mark == 42);
}

TEST_CASE("Test organize_memory() method", "[organize_memory]") {
  typedef Collector<Test_t> collector_t;
  typedef Collector<Test_t>::Ref_t Ref_t;

  static collector_t& collector = collector_t::Collectable::collector;

  SECTION("Testing the function directly") {
    collector.root_list.clear();
    collector.memory.clear();

    Ref_t r1 = collector.add();
    Ref_t r2 = collector.add();
    {
      Ref_t r3 = collector.add();
      Ref_t r4 = collector.add();
      Ref_t r5 = collector.add();
      Ref_t r6 = collector.add();
      REQUIRE(collector.memory.size() == 6);
      REQUIRE(count_mem<Test_t>() == 6);
    }
    REQUIRE(count_mem<Test_t>() == 2);

    Ref_t r7 = collector.add();
    Ref_t r8 = collector.add();
    REQUIRE(count_mem<Test_t>() == 4);
    REQUIRE(collector.memory.size() == 8);

    REQUIRE_NOTHROW(collector.organize_memory());

    REQUIRE(count_mem<Test_t>() == 4);
    REQUIRE(collector.memory.size() == 4);
  }

  SECTION("Testing the through a mark_and_sweep call") {
    collector.root_list.clear();
    collector.memory.clear();

    Ref_t r2 = collector.add();
    {
      // These will be deleted by the
      // reference counting as soon
      // as the scope ends:
      Ref_t r3 = collector.add();
      Ref_t r4 = collector.add();
      Ref_t r5 = collector.add();
      Ref_t r6 = collector.add();
    }
    Ref_t r7 = collector.add();
    Ref_t r8 = collector.add();
    REQUIRE(count_mem<Test_t>() == 3);
    REQUIRE(collector.memory.size() == 7);

    REQUIRE_NOTHROW(collector.mark_and_sweep());

    REQUIRE(count_mem<Test_t>() == 3);
    REQUIRE(collector.memory.size() == 3);
  }
}

/* * * * * Class Level Unit Tests * * * * */

TEST_CASE("Add items", "[memory][add]") {
  SECTION("Add temporary items") {
    REQUIRE(count_mem<Data_t>() == 0);
    {
      // Note: Items declared inside this scope will
      // disappear when it ends and so should their
      // references.
      REQUIRE_NOTHROW(Data({10, "test"}));
      REQUIRE_NOTHROW(Data());
    }
    REQUIRE(count_mem<Data_t>() == 0);
  }

  SECTION("Add items") {
    REQUIRE(count_mem<Data_t>() == 0);
    {
      Data d1({10, "test"}), d2;
      REQUIRE(count_mem<Data_t>() == 2);
    }
    REQUIRE(count_mem<Data_t>() == 0);
  }
}

TEST_CASE("Add root items", "[memory][add][add_root]") {
  static Collector<Data_t>& collector = Collector<Data_t>::Collectable::collector;
  // Note since the Data_t has no possible Data_t childs,
  // (and thus, would not require a mark_and_sweep garbage collector)
  // this test case contain samples of code that might
  // not be useful in a realistic scenario, but are simpler.

  SECTION("Test with a single root member") {
    REQUIRE(count_mem<Data_t>() == 0);
    {
      Data d1;
      REQUIRE_NOTHROW(d1.data = collector.add_root({10, "test"}));
      REQUIRE(count_mem<Data_t>() == 1);
    }
    REQUIRE(count_mem<Data_t>() == 1);
    REQUIRE_NOTHROW(collector.mark_and_sweep());
    REQUIRE(count_mem<Data_t>() == 1);
    collector.root_list.pop_back();
    REQUIRE(count_mem<Data_t>() == 0);
  }

  SECTION("Test with non-root members") {
    REQUIRE(count_mem<Data_t>() == 0);
    {
      Data d1, d2, d3;
      REQUIRE_NOTHROW(d1.data = collector.add_root({10, "test"}));
      REQUIRE_NOTHROW(d2.data = collector.add({1, "testing"}));
      REQUIRE_NOTHROW(d3.data = collector.add({2, "testing"}));
      REQUIRE(count_mem<Data_t>() == 3);
    }
    REQUIRE(count_mem<Data_t>() == 1);

    // Data d1, d2;
    // d1.data = collector.add({1, "testing"});
    // d2.data = collector.add({2, "testing"});
// 
    // REQUIRE(count_mem<Data_t>() == 3);
    // REQUIRE_NOTHROW(collector.mark_and_sweep());
    // REQUIRE(count_mem<Data_t>() == 1);

    collector.root_list.pop_back();
    collector.memory.pop_back();
    collector.memory.pop_back();
    REQUIRE(count_mem<Data_t>() == 0);
  }
}

struct Cycle;
struct Cycle_t : public Collector<Cycle_t>::CollectableData {
  std::string name = "test";

  // Make a reference to this own type
  // making cycles possible:
  typedef Collector<Cycle_t>::Ref_t Ref_t;
  std::shared_ptr<Cycle> cycle;

  Cycle_t() {}
  Cycle_t(std::string name) : name(name) {}
  Cycle_t(std::string name, Cycle& cycle);

  // Necessary for the collector to work:
  void collector_access(Collector<Cycle_t>::callback_t c);
};

struct Cycle : public Collector<Cycle_t>::Collectable {
  Cycle() : Collector<Cycle_t>::Collectable() {}
  Cycle(Cycle_t d) : Collector<Cycle_t>::Collectable(d) {}
  std::string name() { return data->name; }
};
// xxx
Cycle_t::Cycle_t(std::string name, Cycle& cycle) : name(name), cycle(new Cycle(cycle)) {}
void Cycle_t::collector_access(Collector<Cycle_t>::callback_t c) {
  if (cycle.get()) {
    c(&(cycle->data));
  }
}

TEST_CASE("Testing with cycles", "[memory][cycles]") {
  static Collector<Cycle_t>& collector = Collector<Cycle_t>::Collectable::collector;

  SECTION("Test with a single root member") {
    REQUIRE(count_mem<Cycle_t>() == 0);
    {
      Cycle c1;
      REQUIRE_NOTHROW(c1.data = collector.add_root({"test"}));
      REQUIRE(count_mem<Cycle_t>() == 1);
    }
    REQUIRE(count_mem<Cycle_t>() == 1);
    REQUIRE_NOTHROW(collector.mark_and_sweep());
    REQUIRE(count_mem<Cycle_t>() == 1);
    collector.root_list.pop_back();
    REQUIRE(count_mem<Cycle_t>() == 0);
  }

  SECTION("Test with non-root members") {
    REQUIRE(count_mem<Cycle_t>() == 0);
    {
      Cycle c1, c2, c3;
      REQUIRE_NOTHROW(c1.data = collector.add({"child1"}));
      REQUIRE_NOTHROW(c2.data = collector.add({"child2", c1}));
      REQUIRE_NOTHROW(c3.data = collector.add_root({"root", c2}));
      REQUIRE(count_mem<Cycle_t>() == 3);
    }
    REQUIRE(count_mem<Cycle_t>() == 3);

    REQUIRE_NOTHROW(collector.mark_and_sweep());
    REQUIRE(count_mem<Cycle_t>() == 3);

    // Cut the tail of the list:
    collector.root_list[0].reset();

    // Since we are counting non-resetted references,
    // there should be none available now:
    REQUIRE(count_mem<Cycle_t>() == 0);
    collector.root_list.pop_back();
    REQUIRE(count_mem<Cycle_t>() == 0);
  }

  SECTION("Test with an actual cycle") {
    REQUIRE(count_mem<Cycle_t>() == 0);
    {
      Cycle c1, c2, c3;
      REQUIRE_NOTHROW(c1.data = collector.add({"child1"}));
      REQUIRE_NOTHROW(c2.data = collector.add({"child2", c1}));
      REQUIRE_NOTHROW(c3.data = collector.add_root({"root", c2}));

      // Make the cycle between c1 and c2:
      REQUIRE_NOTHROW(c1.data->cycle = std::shared_ptr<Cycle>(new Cycle(c2)));
      REQUIRE(count_mem<Cycle_t>() == 3);
    }
    REQUIRE(count_mem<Cycle_t>() == 3);

    REQUIRE_NOTHROW(collector.mark_and_sweep());
    REQUIRE(count_mem<Cycle_t>() == 3);

    // Cut the tail of the linked list:
    collector.root_list[0].reset();

    // There is a cycle so the reference counting
    // will not delete the tail:
    REQUIRE(count_mem<Cycle_t>() == 2);

    // But the mark and sweep will:
    REQUIRE_NOTHROW(collector.mark_and_sweep());
    REQUIRE(count_mem<Cycle_t>() == 0);

    collector.root_list.pop_back();
    REQUIRE(count_mem<Cycle_t>() == 0);
  }
}

}  // namespace test

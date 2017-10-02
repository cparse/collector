#include <vector>
#include <memory>

#ifdef DEBUG
#define private public
#endif

#ifndef ORGANIZATION_THRESHOLD
#define ORGANIZATION_THRESHOLD 0.5
#endif

namespace collector {

typedef bool any_type;

template <typename T>
struct Collector {
 public:
  // Stores the current mark used.
  // The mark() function will add 1 to it
  // and then mark every reachable item_t with
  // the updated mark.
  static uint8_t curr_mark;

 public:
  typedef std::weak_ptr<T> wRef_t;
  typedef std::shared_ptr<T> Ref_t;
  typedef std::vector<wRef_t> memory_t;
  typedef std::vector<Ref_t> root_list_t;
  typedef void (*callback_t)(Ref_t*);

 public:
  // Superclass of the data types:
  struct CollectableData {
    uint8_t collector_mark = 0;

    // Should apply the callback to each other
    // Collectable contained within this container.
    virtual void collector_access(callback_t) = 0;
  };

  // Superclass necessary for items
  // to be managed by the collector:
  struct Collectable {
    typedef std::shared_ptr<T> Ref_t;
    static Collector<T> collector;
    Ref_t data;

    Collectable(T t = T()) : data(collector.add(t)) {}
  };

 private:
  memory_t memory;
  root_list_t root_list;

 public:
  inline Ref_t add_root(T value = T()) {
    Ref_t item = std::make_shared<T>(value);
    root_list.emplace_back(item);
    // Return a Ref_t built with the alias constructor:
    return item;
  }

  inline Ref_t add(T value = T()) {
    Ref_t item = std::make_shared<T>(value);
    memory.emplace_back(item);
    // Return a Ref_t built with the alias constructor:
    return item;
  }

  void mark_and_sweep() {
    // Refresh the curr_mark:
    ++Collector<any_type>::curr_mark;
    // Mark all reachable tokens with the new mark:
    mark();
    // Delete any token that could not be marked:
    sweep();

    // Organize:
    if (count_empty_space() > memory.size() * ORGANIZATION_THRESHOLD) {
      organize_memory();
    }
  }

 private:
  void mark() {
    for (Ref_t& ref : root_list) {
      mark_callback(&ref);
    }
  }

  void sweep() {
    // Run a sweep algorithm:
    for (const wRef_t& wref : memory) {
      Ref_t ref = wref.lock();
      // If the original shared_ptr still exists and is not marked:
      if (ref && ref->collector_mark != Collector<any_type>::curr_mark) {
        // Reset each pointer contained within it:
        ref->collector_access(Collector::reset_callback);

        // Doing this will not delete `ref` directly but will trigger
        // a deallocation cascade, i.e. it will delete all children
        // of `ref`, thus breaking the cycle that kept it allocated
        // beyond the reach of the reference counting system.
        //
        // Note that the `ref` pointer will not be deleted on the
        // cascade because we still have at least the current
        // reference to it.
      }
      // When we leave the loop `ref` loses its last reference
      // and should be deleted.
    }
  }

  size_t count_empty_space() {
    size_t empty_refs = 0;

    for (const wRef_t& wref : memory) {
      Ref_t ref = wref.lock();

      if (!ref) {
        empty_refs++;
      }
    }

    return empty_refs;
  }

  void organize_memory() {
    int next = 0;

    for (const wRef_t& wref : memory) {
      Ref_t ref = wref.lock();

      if (ref) {
        memory[next++] = wref;
      }
    }

    memory.resize(next);
  }

 private:
  static void mark_callback(Ref_t* ref) {
    // If it is empty or already marked:
    if (ref->get() == 0 || ref->get()->collector_mark == Collector<any_type>::curr_mark) {
      return;
    }

    // Mark it and mark its contents:
    ref->get()->collector_mark = Collector<any_type>::curr_mark;
    ref->get()->collector_access(Collector::mark_callback);
  }

  static void reset_callback(Ref_t* ref) {
    ref->reset();
  }
};

template<typename T>
uint8_t Collector<T>::curr_mark = 0;

template <typename T>
Collector<T> Collector<T>::Collectable::collector;

}  // namespace collector

# collector

A generic single header garbage collector that mixes reference counting and a mark &amp; sweep algorithm

This library is not intended to provide a garbage collector to C++ itself (like the [Boehm collector][boehm] project),
but rather to provide generic garbage collection for data types when developing a new programming language in C++.

[boehm]: http://hboehm.info/gc/

## Usage:

To create a garbage collected data type using this library you'll need 2 separate classes:

```C++
// The actual class:
class LinkedList;

// A separated data class containing all data:
struct Data_t : public Collector<Data_t>::CollectableData {
  std::string name = "test";

  // Make a reference to this own type
  // making cycles possible:
  typedef Collector<Data_t>::Ref_t Ref_t;
  std::shared_ptr<LinkedList> next;

  Data_t() {}
  Data_t(std::string name) : name(name) {}
  Data_t(std::string name, LinkedList& next);

  // Necessary for the collector to work:
  void collector_access(Collector<Data_t>::callback_t c);
};

// The actual class implementation:
struct LinkedList : public Collector<Data_t>::Collectable {
  LinkedList() : Collector<Data_t>::Collectable() {}
  LinkedList(Data_t d) : Collector<Data_t>::Collectable(d) {}
  std::string name() { return data->name; }
};

Data_t::Data_t(std::string name, LinkedList& next) : name(name), next(new LinkedList(next)) {}
```

You will also have to implement the `collector_access()` virtual function to allow the collector
to have access to any contained items that might need to be garbage collected:

```C++
void Data_t::collector_access(Collector<Data_t>::callback_t c) {
  if (next.get()) {
    c(&(next->data));
  }
}
```

*An observation:*

- If your type will not contain references that can generate cycles you don't need a complete
  garbage collector, use `shared_ptr`s instead.

## Implementation Details

This collector provides both a Reference Counting mechanism and a Mark and Sweep algorithm.

This means that most of the time your data will be deleted when the number of items referencing it get to zero.
However, if you know a little about garbage collecting you know that reference counting don't work when the data
contain cycles.

For these cases we provide the `Collector<T>::mark_and_sweep()` function which is not trivial to use.

To use it you need 3 things:

- Choose the root items with the `Collector<T>::add_root()` function.
- Add the items that should be garbage collected with the `Collector<T>::add()` function.
- Call the `Collector<T>::mark_and_sweep()` function from time to time.

Every time you call the mark and sweep function, it will do 2 things:

1. Starting from the root nodes mark with a unique number every item it can reach
   using the `collector_access` function you should have implemented earlier.
2. Afterwards it will go through the garbage collected items you've added with `add()`
   and reset any references they make to other nodes focing any existing cycle to break.

So note that there will be 3 types of items:

1. The ones you created yourself and that will be the root containers for all the data of
   the programming language.
2. The items that will be created by the programming language code and that will require garbage collection.
3. Instances of the class you created yourself for some purpose and that will not be used by the programming language.

The **first** group should be added to the collector with the `add_root()` function.

The **second** group should be created if and only if there is a reference from a root node that gets to them,
since otherwise they will be resetted on the first call to `mark_and_sweep()`.

The **third** group should not be added at all, if you add it as root you will be unnecessarily increasing the number
of operations of the garbage collector and if you add it as a non-root node any child items contained within it might
end up being `sweep()`'ed by the GC if there is no reference from a root node.

> This also means that you should not keep references to items from the second group outside the programming language
> when you call the `mark_and_sweep` function.

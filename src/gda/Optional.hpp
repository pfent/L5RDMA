//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - This helps ensuring that optional fields are only read if they were set before.
// -------------------------------------------------------------------------------------------------
#include <stdexcept>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
template<class T>
class Optional {
public:
   Optional() : isSet(false) { }
   Optional(T value) : isSet(true), value(value) { }

   operator T() const { ensure(); return value; }
   const T& get() const { ensure(); return value; }
   const Optional& operator=(const T& value) { isSet = true; this->value = value; return *this; }
   const Optional& operator=(T&& value) { isSet = true; this->value = std::move(value); return *this; }

   bool isValueSet() const { return isSet; }

private:
   bool isSet;
   T value;

   void ensure() const { if (!isSet) throw std::runtime_error("Optional value was not set, but read."); }
};
// -------------------------------------------------------------------------------------------------
// eq
template<class T, class O>
inline bool operator==(const Optional<T>& t, O& o) { return t.get() == o; }
template<class T, class O>
inline bool operator==(const Optional<T>& t, const Optional<O>& o) { return t.get() == o.get(); }
// -------------------------------------------------------------------------------------------------
// neq
template<class T, class O>
inline bool operator!=(const Optional<T>& t, O& o) { return t.get() != o; }
template<class T, class O>
inline bool operator!=(const Optional<T>& t, const Optional<O>& o) { return t.get() != o.get(); }
// -------------------------------------------------------------------------------------------------
// lt
template<class T, class O>
inline bool operator<(const Optional<T>& t, O& o) { return t.get() < o; }
template<class T, class O>
inline bool operator<(const Optional<T>& t, const Optional<O>& o) { return t.get() < o.get(); }
// -------------------------------------------------------------------------------------------------
// gt
template<class T, class O>
inline bool operator>(const Optional<T>& t, O& o) { return t.get() > o; }
template<class T, class O>
inline bool operator>(const Optional<T>& t, const Optional<O>& o) { return t.get() > o.get(); }
// -------------------------------------------------------------------------------------------------
// leq
template<class T, class O>
inline bool operator<=(const Optional<T>& t, O& o) { return t.get() <= o; }
template<class T, class O>
inline bool operator<=(const Optional<T>& t, const Optional<O>& o) { return t.get() <= o.get(); }
// -------------------------------------------------------------------------------------------------
// geq
template<class T, class O>
inline bool operator>=(const Optional<T>& t, O& o) { return t.get() >= o; }
template<class T, class O>
inline bool operator>=(const Optional<T>& t, const Optional<O>& o) { return t.get() >= o.get(); }
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------

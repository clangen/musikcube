/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include <algorithm>
#include <memory>

namespace TagLib {

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

// The functionality of List<T>::setAutoDelete() is implemented here partial
// template specialization.  This is implemented in such a way that calling
// setAutoDelete() on non-pointer types will simply have no effect.

// A base for the generic and specialized private class types.  New
// non-templatized members should be added here.

class ListPrivateBase
{
public:
  bool autoDelete{};
};

// A generic implementation

template <class T>
template <class TP> class List<T>::ListPrivate  : public ListPrivateBase
{
public:
  using ListPrivateBase::ListPrivateBase;
  ListPrivate(const std::list<TP> &l) : list(l) {}
  ListPrivate(std::initializer_list<TP> init) : list(init) {}
  void clear() {
    list.clear();
  }
  std::list<TP> list;
};

// A partial specialization for all pointer types that implements the
// setAutoDelete() functionality.

template <class T>
template <class TP> class List<T>::ListPrivate<TP *> : public ListPrivateBase
{
public:
  using ListPrivateBase::ListPrivateBase;
  ListPrivate(const std::list<TP *> &l) : list(l) {}
  ListPrivate(std::initializer_list<TP *> init) : list(init) {}
  ~ListPrivate() {
    clear();
  }
  ListPrivate(const ListPrivate &) = delete;
  ListPrivate &operator=(const ListPrivate &) = delete;
  void clear() {
    if(autoDelete) {
      for(auto &m : list)
        delete m;
    }
    list.clear();
  }
  std::list<TP *> list;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

template <class T>
List<T>::List() :
  d(std::make_shared<ListPrivate<T>>())
{
}

template <class T>
List<T>::List(const List<T> &) = default;

template <class T>
List<T>::List(std::initializer_list<T> init) :
  d(std::make_shared<ListPrivate<T>>(init))
{
}

template <class T>
List<T>::~List() = default;

template <class T>
typename List<T>::Iterator List<T>::begin()
{
  detach();
  return d->list.begin();
}

template <class T>
typename List<T>::ConstIterator List<T>::begin() const
{
  return d->list.begin();
}

template <class T>
typename List<T>::ConstIterator List<T>::cbegin() const
{
  return d->list.cbegin();
}

template <class T>
typename List<T>::Iterator List<T>::end()
{
  detach();
  return d->list.end();
}

template <class T>
typename List<T>::ConstIterator List<T>::end() const
{
  return d->list.end();
}

template <class T>
typename List<T>::ConstIterator List<T>::cend() const
{
  return d->list.cend();
}

template <class T>
typename List<T>::Iterator List<T>::insert(Iterator it, const T &item)
{
  return d->list.insert(it, item);
}

template <class T>
List<T> &List<T>::sortedInsert(const T &value, bool unique)
{
  detach();
  Iterator it = begin();
  while(it != end() && *it < value)
    ++it;
  if(unique && it != end() && *it == value)
    return *this;
  insert(it, value);
  return *this;
}

template <class T>
List<T> &List<T>::append(const T &item)
{
  detach();
  d->list.push_back(item);
  return *this;
}

template <class T>
List<T> &List<T>::append(const List<T> &l)
{
  detach();
  d->list.insert(d->list.end(), l.begin(), l.end());
  return *this;
}

template <class T>
List<T> &List<T>::prepend(const T &item)
{
  detach();
  d->list.push_front(item);
  return *this;
}

template <class T>
List<T> &List<T>::prepend(const List<T> &l)
{
  detach();
  d->list.insert(d->list.begin(), l.begin(), l.end());
  return *this;
}

template <class T>
List<T> &List<T>::clear()
{
  detach();
  d->clear();
  return *this;
}

template <class T>
unsigned int List<T>::size() const
{
  return static_cast<unsigned int>(d->list.size());
}

template <class T>
bool List<T>::isEmpty() const
{
  return d->list.empty();
}

template <class T>
typename List<T>::Iterator List<T>::find(const T &value)
{
  detach();
  return std::find(d->list.begin(), d->list.end(), value);
}

template <class T>
typename List<T>::ConstIterator List<T>::find(const T &value) const
{
  return std::find(d->list.begin(), d->list.end(), value);
}

template <class T>
typename List<T>::ConstIterator List<T>::cfind(const T &value) const
{
  return std::find(d->list.cbegin(), d->list.cend(), value);
}

template <class T>
bool List<T>::contains(const T &value) const
{
  return std::find(d->list.begin(), d->list.end(), value) != d->list.end();
}

template <class T>
typename List<T>::Iterator List<T>::erase(Iterator it)
{
  return d->list.erase(it);
}

template <class T>
const T &List<T>::front() const
{
  return d->list.front();
}

template <class T>
T &List<T>::front()
{
  detach();
  return d->list.front();
}

template <class T>
const T &List<T>::back() const
{
  return d->list.back();
}

template <class T>
void List<T>::setAutoDelete(bool autoDelete)
{
  detach();
  d->autoDelete = autoDelete;
}

template <class T>
bool List<T>::autoDelete() const
{
  return d->autoDelete;
}

template <class T>
T &List<T>::back()
{
  detach();
  return d->list.back();
}

template <class T>
T &List<T>::operator[](unsigned int i)
{
  auto it = d->list.begin();
  std::advance(it, i);

  return *it;
}

template <class T>
const T &List<T>::operator[](unsigned int i) const
{
  auto it = d->list.begin();
  std::advance(it, i);

  return *it;
}

template <class T>
List<T> &List<T>::operator=(const List<T> &) = default;

template <class T>
List<T> &List<T>::operator=(std::initializer_list<T> init)
{
  bool autoDeleteEnabled = d->autoDelete;
  List(init).swap(*this);
  setAutoDelete(autoDeleteEnabled);
  return *this;
}

template <class T>
void List<T>::swap(List<T> &l) noexcept
{
  using std::swap;

  swap(d, l.d);
}

template <class T>
bool List<T>::operator==(const List<T> &l) const
{
  return d->list == l.d->list;
}

template <class T>
bool List<T>::operator!=(const List<T> &l) const
{
  return d->list != l.d->list;
}

template <class T>
void List<T>::sort()
{
  detach();
  d->list.sort();
}

template <class T>
template <class Compare>
void List<T>::sort(Compare&& comp)
{
  detach();
  d->list.sort(std::forward<Compare>(comp));
}

////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

template <class T>
void List<T>::detach()
{
  if(d.use_count() > 1) {
    d = std::make_shared<ListPrivate<T>>(d->list);
  }
}

} // namespace TagLib

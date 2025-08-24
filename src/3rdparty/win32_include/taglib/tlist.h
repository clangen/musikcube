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

#ifndef TAGLIB_LIST_H
#define TAGLIB_LIST_H

#include <list>
#include <initializer_list>
#include <memory>

namespace TagLib {

  //! A generic, implicitly shared list.

  /*!
   * This is a basic generic list that's somewhere between a std::list and a
   * QList.  This class is implicitly shared.  For example:
   *
   * \code
   *
   * TagLib::List<int> l = someOtherIntList;
   *
   * \endcode
   *
   * The above example is very cheap.  This also makes lists suitable as
   * return types of functions.  The above example will just copy a pointer rather
   * than copying the data in the list.  When your \e shared list's data changes,
   * only \e then will the data be copied.
   */

  template <class T> class List
  {
  public:
#ifndef DO_NOT_DOCUMENT
    using Iterator = typename std::list<T>::iterator;
    using ConstIterator = typename std::list<T>::const_iterator;
#endif

    /*!
     * Constructs an empty list.
     */
    List();

    /*!
     * Make a shallow, implicitly shared, copy of \a l.  Because this is
     * implicitly shared, this method is lightweight and suitable for
     * pass-by-value usage.
     */
    List(const List<T> &l);

    /*!
     * Construct a List with the contents of the braced initializer list.
     */
    List(std::initializer_list<T> init);

    /*!
     * Destroys this List instance.  If auto deletion is enabled and this list
     * contains a pointer type, all of the members are also deleted.
     */
    ~List();

    /*!
     * Returns an STL style iterator to the beginning of the list.  See
     * \c std::list::const_iterator for the semantics.
     */
    Iterator begin();

    /*!
     * Returns an STL style constant iterator to the beginning of the list.  See
     * \c std::list::iterator for the semantics.
     */
    ConstIterator begin() const;

    /*!
     * Returns an STL style constant iterator to the beginning of the list.  See
     * \c std::list::iterator for the semantics.
     */
    ConstIterator cbegin() const;

    /*!
     * Returns an STL style iterator to the end of the list.  See
     * \c std::list::iterator for the semantics.
     */
    Iterator end();

    /*!
     * Returns an STL style constant iterator to the end of the list.  See
     * \c std::list::const_iterator for the semantics.
     */
    ConstIterator end() const;

    /*!
     * Returns an STL style constant iterator to the end of the list.  See
     * \c std::list::const_iterator for the semantics.
     */
    ConstIterator cend() const;

    /*!
     * Inserts a copy of \a item before \a it.
     *
     * \note This method cannot detach because \a it is tied to the internal
     * list.  Do not make an implicitly shared copy of this list between
     * getting the iterator and calling this method!
     */
    Iterator insert(Iterator it, const T &item);

    /*!
     * Inserts the \a value into the list.  This assumes that the list is
     * currently sorted.  If \a unique is \c true then the value will not
     * be inserted if it is already in the list.
     */
    List<T> &sortedInsert(const T &value, bool unique = false);

    /*!
     * Appends \a item to the end of the list and returns a reference to the
     * list.
     */
    List<T> &append(const T &item);

    /*!
     * Appends all of the values in \a l to the end of the list and returns a
     * reference to the list.
     */
    List<T> &append(const List<T> &l);

    /*!
     * Prepends \a item to the beginning list and returns a reference to the
     * list.
     */
    List<T> &prepend(const T &item);

    /*!
     * Prepends all of the items in \a l to the beginning list and returns a
     * reference to the list.
     */
    List<T> &prepend(const List<T> &l);

    /*!
     * Clears the list.  If auto deletion is enabled and this list contains a
     * pointer type the members are also deleted.
     *
     * \see setAutoDelete()
     */
    List<T> &clear();

    /*!
     * Returns the number of elements in the list.
     *
     * \see isEmpty()
     */
    unsigned int size() const;

    /*!
     * Returns whether or not the list is empty.
     *
     * \see size()
     */
    bool isEmpty() const;

    /*!
     * Find the first occurrence of \a value.
     */
    Iterator find(const T &value);

    /*!
     * Find the first occurrence of \a value.
     */
    ConstIterator find(const T &value) const;

    /*!
     * Find the first occurrence of \a value.
     */
    ConstIterator cfind(const T &value) const;

    /*!
     * Returns \c true if the list contains \a value.
     */
    bool contains(const T &value) const;

    /*!
     * Erase the item at \a it from the list.
     *
     * \note This method cannot detach because \a it is tied to the internal
     * list.  Do not make an implicitly shared copy of this list between
     * getting the iterator and calling this method!
     */
    Iterator erase(Iterator it);

    /*!
     * Returns a reference to the first item in the list.
     */
    const T &front() const;

    /*!
     * Returns a reference to the first item in the list.
     */
    T &front();

    /*!
     * Returns a reference to the last item in the list.
     */
    const T &back() const;

    /*!
     * Returns a reference to the last item in the list.
     */
    T &back();

    /*!
     * Auto delete the members of the list when the last reference to the list
     * passes out of scope.  This will have no effect on lists which do not
     * contain a pointer type.
     *
     * \note This relies on partial template instantiation -- most modern C++
     * compilers should now support this.
     */
    void setAutoDelete(bool autoDelete);

    /*!
     * Returns \c true if auto-deletion is enabled.
     */
    bool autoDelete() const;

    /*!
     * Returns a reference to item \a i in the list.
     *
     * \warning This method is slow.  Use iterators to loop through the list.
     */
    T &operator[](unsigned int i);

    /*!
     * Returns a const reference to item \a i in the list.
     *
     * \warning This method is slow.  Use iterators to loop through the list.
     */
    const T &operator[](unsigned int i) const;

    /*!
     * Make a shallow, implicitly shared, copy of \a l.  Because this is
     * implicitly shared, this method is lightweight and suitable for
     * pass-by-value usage.
     */
    List<T> &operator=(const List<T> &l);

    /*!
     * Replace the contents of the list with those of the braced initializer list.
     *
     * If auto deletion is enabled and the list contains a pointer type, the members are also deleted
     */
    List<T> &operator=(std::initializer_list<T> init);

    /*!
     * Exchanges the content of this list with the content of \a l.
     */
    void swap(List<T> &l) noexcept;

    /*!
     * Compares this list with \a l and returns \c true if all of the elements are
     * the same.
     */
    bool operator==(const List<T> &l) const;

    /*!
     * Compares this list with \a l and returns \c true if the lists differ.
     */
    bool operator!=(const List<T> &l) const;

    /*!
     * Sorts this list in ascending order using operator< of T.
     */
    void sort();

    /*!
     * Sorts this list in ascending order using the comparison
     * function object \a comp which returns \c true if the first argument is
     * less than the second.
     */
    template<class Compare>
    void sort(Compare&& comp);

  protected:
    /*!
     * If this List is being shared via implicit sharing, do a deep copy of the
     * data and separate from the shared members.  This should be called by all
     * non-const subclass members without Iterator parameters.
     */
    void detach();

  private:
#ifndef DO_NOT_DOCUMENT
    template <class TP> class ListPrivate;
    std::shared_ptr<ListPrivate<T>> d;
#endif
  };

}  // namespace TagLib

// Since GCC doesn't support the "export" keyword, we have to include the
// implementation.

#include "tlist.tcc"

#endif

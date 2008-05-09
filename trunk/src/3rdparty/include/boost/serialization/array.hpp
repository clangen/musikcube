#ifndef BOOST_SERIALIZATION_ARRAY_HPP
#define BOOST_SERIALIZATION_ARRAY_HPP

// (C) Copyright 2005 Matthias Troyer and Dave Abrahams
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/wrapper.hpp>
#include <boost/serialization/collection_size_type.hpp>
#include <boost/archive/archive_exception.hpp>
#include <boost/throw_exception.hpp>
#include <iostream>

namespace boost { namespace serialization {

template<class T>
class array
 : public wrapper_traits<array<T> >
{
public:    
    typedef T value_type;
    
    array(value_type* t, std::size_t s) :
        m_t(t),
        m_element_count(s)
    {}
    
    // default implementation
    template<class Archive>
    void serialize(Archive &ar, const unsigned int) const
    {
      // default implemention does the loop
      std::size_t c = count();
      value_type * t = address();
      while(0 < c--)
            ar & make_nvp("item", *t++);
    }
    
    value_type* address() const
    {
      return m_t;
    }

    std::size_t count() const
    {
      return m_element_count;
    }
    
    
private:
    value_type* m_t;
    std::size_t const m_element_count;
};

template<class T>
inline
#ifndef BOOST_NO_FUNCTION_TEMPLATE_ORDERING
const
#endif
array<T> make_array( T* t, std::size_t s){
    return array<T>(t, s);
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// T[N]

/*

template<class Archive, class U, std::size_t N>
void save( Archive & ar, U const (& t)[N], const unsigned int file_version )
{
  const serialization::collection_size_type count(N);
  ar << BOOST_SERIALIZATION_NVP(count);
  if (N)
    ar << serialization::make_array(&t[0],N);
}

template<class Archive, class U, std::size_t N>
void load( Archive & ar, U (& t)[N], const unsigned int file_version )
{
  serialization::collection_size_type count;
  ar >> BOOST_SERIALIZATION_NVP(count);
  if(count > N)
      boost::throw_exception(archive::archive_exception(
        boost::archive::archive_exception::array_size_too_short
      ));
  if (N)
    ar >> serialization::make_array(&t[0],count);
}


// split non-intrusive serialization function member into separate
// non intrusive save/load member functions
template<class Archive, class U, std::size_t N>
inline void serialize( Archive & ar, U (& t)[N], const unsigned int file_version)
{
    boost::serialization::split_free(ar, t, file_version);
}
*/


} } // end namespace boost::serialization

#endif //BOOST_SERIALIZATION_ARRAY_HPP

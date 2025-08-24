/***************************************************************************
    copyright            : (C) 2020 by Kevin Andre
    email                : hyperquantum@gmail.com

    copyright            : (C) 2023 by Urs Fleisch
    email                : ufleisch@users.sourceforge.net
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

#ifndef TAGLIB_VERSIONNUMBER_H
#define TAGLIB_VERSIONNUMBER_H

#include "taglib_export.h"

namespace TagLib {

  class String;

  //! Version number with major, minor and patch segments.

  class TAGLIB_EXPORT VersionNumber {
  public:
    /*!
     * Constructs a version number from \a major, \a minor and \a patch segments.
     */
    constexpr VersionNumber(unsigned int major, unsigned int minor,
                            unsigned int patch = 0)
      : m_combined(((major & 0xff) << 16) | ((minor & 0xff) << 8)
                  | (patch & 0xff)) {
    }

    /*!
     * Returns the version as an unsigned integer in the form
     * (major version << 16) | (minor version << 8) | (patch version),
     * e.g. 0x020100 for version 2.1.0.
     */
    constexpr unsigned int combinedVersion() const {
      return m_combined;
    }

    /*!
     * Returns the major version, e.g. 2
     */
    constexpr unsigned int majorVersion() const {
      return (m_combined & 0xff0000) >> 16;
    }

    /*!
     * Returns the minor version, e.g. 1
     */
    constexpr unsigned int minorVersion() const {
      return (m_combined & 0xff00) >> 8;
    }

    /*!
     * Returns the patch version, e.g. 0
     */
    constexpr unsigned int patchVersion() const {
      return m_combined & 0xff;
    }

    /*!
     * Returns \c true if this version is equal to \a rhs.
     */
    constexpr bool operator==(const VersionNumber &rhs) const {
      return m_combined == rhs.m_combined;
    }

    /*!
     * Returns \c true if this version is not equal to \a rhs.
     */
    constexpr bool operator!=(const VersionNumber &rhs) const {
      return m_combined != rhs.m_combined;
    }

    /*!
     * Returns \c true if this version is less than \a rhs.
     */
    constexpr bool operator<(const VersionNumber &rhs) const {
      return m_combined < rhs.m_combined;
    }

    /*!
     * Returns \c true if this version is greater than \a rhs.
     */
    constexpr bool operator>(const VersionNumber &rhs) const {
      return m_combined > rhs.m_combined;
    }

    /*!
     * Returns \c true if this version is less or equal than \a rhs.
     */
    constexpr bool operator<=(const VersionNumber &rhs) const {
      return m_combined <= rhs.m_combined;
    }

    /*!
     * Returns \c true if this version is greater or equal than \a rhs.
     */
    constexpr bool operator>=(const VersionNumber &rhs) const {
      return m_combined >= rhs.m_combined;
    }

    /*!
     * Returns a string with major, minor, and patch versions separated by
     * periods.
     */
    String toString() const;

  private:
    unsigned int m_combined;
  };

  /*!
   * \relates TagLib::VersionNumber
   * Returns the version number of TagLib in use at runtime.
   * This does not need not be the version the application was compiled with.
   */
  TAGLIB_EXPORT VersionNumber runtimeVersion();

}  // namespace TagLib

#endif

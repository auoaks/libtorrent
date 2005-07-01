// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DOWNLOAD_CHOKE_MANAGER_H
#define LIBTORRENT_DOWNLOAD_CHOKE_MANAGER_H

#include <list>

#include "utils/unordered_vector.h"

namespace torrent {

class PeerConnectionBase;

class ChokeManager {
public:
  typedef unordered_vector<PeerConnectionBase*> Container;
  typedef Container::iterator                   iterator;

  ChokeManager() : m_maxUnchoked(15), m_minGenerous(2), m_cycleSize(2) {}
  
  int                 get_max_unchoked() const                { return m_maxUnchoked; }
  void                set_max_unchoked(int v)                 { m_maxUnchoked = v; }

  int                 get_min_generous() const                { return m_minGenerous; }
  void                set_min_generous(int v)                 { m_minGenerous = v; }

  int                 get_cycle_size() const                  { return m_cycleSize; }
  void                set_cycle_size(int v)                   { m_cycleSize = v; }

  int                 get_unchoked(iterator first, iterator last) const;

  void                balance(iterator first, iterator last);
  void                cycle(iterator first, iterator last);

  void                choke(iterator first, iterator last, int count);
  void                unchoke(iterator first, iterator last, int count);

private:
  static iterator     seperate_interested(iterator first, iterator last);
  static iterator     seperate_unchoked(iterator first, iterator last);

  static void         sort_read_rate(iterator first, iterator last);

  int                 m_maxUnchoked;
  int                 m_minGenerous;
  int                 m_cycleSize;
};

}

#endif
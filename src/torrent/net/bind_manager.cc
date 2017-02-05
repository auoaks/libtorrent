// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include "config.h"

#include <cerrno>
#include <cstring>
#include <cinttypes>

#include "bind_manager.h"

#include "net/socket_fd.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

// Deprecated.
#include "rak/socket_address.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_CONNECTION_BIND, "bind: " log_fmt, __VA_ARGS__);
#define LT_LOG_SOCKADDR(log_fmt, sa, ...)                               \
  lt_log_print(LOG_CONNECTION_BIND, "bind->%s: " log_fmt, sa_pretty_address_str(sa).c_str(), __VA_ARGS__);

namespace torrent {

bind_struct
make_bind_struct(const sockaddr* sa, int f, uint16_t priority) {
  return bind_struct { f, sa_copy(sa), priority, 0, 0 };
}

bind_manager::bind_manager() {
  // TODO: Move this to a different place.
  base_type::push_back(make_bind_struct(NULL, flag_default, 0));
}

void
bind_manager::add_bind(const sockaddr* bind_sockaddr, int flags) {
  if (!sa_is_bindable(bind_sockaddr)) {
    LT_LOG("add bind failed, address is not bindable (flags:0x%x address:%s)",
           flags, sa_pretty_address_str(bind_sockaddr).c_str());

    // Throw here?
    return;
  }

  LT_LOG("bind added (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(bind_sockaddr).c_str());

  // TODO: Add a way to set the order.
  base_type::push_back(make_bind_struct(bind_sockaddr, flags, 0));
}

static bool
attempt_connect(int file_desc, const sockaddr* connect_sockaddr, const sockaddr* bind_sockaddr) {
  SocketFd socket_fd(file_desc);

  if (!sa_is_default(bind_sockaddr) && !socket_fd.bind_sa(bind_sockaddr)) {
    LT_LOG_SOCKADDR("bind failed (fd:%i address:%s errno:%i message:'%s')",
                    bind_sockaddr, file_desc, sa_pretty_address_str(connect_sockaddr).c_str(), errno, std::strerror(errno));
    return false;
  }

  if (!socket_fd.connect_sa(connect_sockaddr)) {
    LT_LOG_SOCKADDR("connect failed (fd:%i address:%s errno:%i message:'%s')",
                    bind_sockaddr, file_desc, sa_pretty_address_str(connect_sockaddr).c_str(), errno, std::strerror(errno));
    return false;
  }

  LT_LOG_SOCKADDR("connect success (fd:%i address:%s)",
                  bind_sockaddr, file_desc, sa_pretty_address_str(connect_sockaddr).c_str());

  return true;
}

int
bind_manager::connect_socket(const sockaddr* connect_sockaddr, int flags, alloc_fd_ftor alloc_fd) const {
  // Santitize flags.

  LT_LOG("connect_socket attempt (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(connect_sockaddr).c_str());

  for (auto& itr : *this) {
    int file_desc = alloc_fd();

    if (file_desc == -1) {
      LT_LOG("connect_socket open failed (address:%s errno:%i message:'%s')",
             sa_pretty_address_str(connect_sockaddr).c_str(), errno, std::strerror(errno));

      return -1;
    }

    if (attempt_connect(file_desc, connect_sockaddr, itr.address.get())) {
      LT_LOG("connect_socket succeeded (flags:0x%x address:%s fd:%i)",
             flags, sa_pretty_address_str(connect_sockaddr).c_str(), file_desc);

      return file_desc;
    }

    fd_close(file_desc);
  }

  LT_LOG("connect_socket failed (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(connect_sockaddr).c_str());

  return -1;
}

static int
attempt_listen(const sockaddr* bind_sockaddr, uint16_t port_first, uint16_t port_last, int backlog, bind_manager::listen_fd_type listen_fd) {
  std::unique_ptr<sockaddr> sa;

  if (sa_is_default(bind_sockaddr))
    sa = sa_make_inet6();
  else
    sa = sa_copy(bind_sockaddr);

  int fd = fd_open(fd_flag_stream | fd_flag_nonblock | fd_flag_reuse_address);

  if (fd == -1) {
    LT_LOG_SOCKADDR("listen_socket open failed (errno:%i message:'%s')",
                    sa.get(), errno, std::strerror(errno));
    return -1;
  }

  for (uint16_t port = port_first; port != port_last; port++) {
    sa_set_port(sa.get(), port);

    if (!fd_bind(fd, sa.get()))
      continue;

    if (!fd_listen(fd, backlog)) {
      LT_LOG_SOCKADDR("call to listen failed (fd:%i backlog:%i errno:%i message:'%s')",
                      sa.get(), fd, backlog, errno, std::strerror(errno));
      fd_close(fd);
      return -1;
    }

    listen_fd(fd, sa.get());

    LT_LOG_SOCKADDR("listen success (fd:%i)", sa.get(), fd);
    return fd;
  }

  LT_LOG_SOCKADDR("listen failed (fd:%i port_first:%" PRIu16 " port_last:%" PRIu16 ")",
                  sa.get(), fd, port_first, port_last);
  return -1;
}

int
bind_manager::listen_socket(uint16_t port_first, uint16_t port_last, int backlog, int flags, listen_fd_type listen_fd) const {
  LT_LOG("listen_socket attempt (flags:0x%x port_first:%" PRIu16 " port_last:%" PRIu16 ")",
         flags, port_first, port_last);

  for (auto& itr : *this) {
    int fd = attempt_listen(itr.address.get(), port_first, port_last, backlog, listen_fd);

    if (fd == -1)
      continue;

    LT_LOG("listen_socket succeeded (flags:0x%x fd:%i port_first:%" PRIu16 " port_last:%" PRIu16 ")",
           flags, fd, port_first, port_last);
    return fd;
  }

  LT_LOG("listen_socket failed (flags:0x%x port_first:%" PRIu16 " port_last:%" PRIu16 ")",
         flags, port_first, port_last);
  return -1;
}

}

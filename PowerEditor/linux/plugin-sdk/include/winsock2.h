// This file is part of Notepad++ project
// Copyright (C)2026 Notepad++ Linux port contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Notepad++ Linux plugin SDK: winsock2.h. Plugins that only include it get windows.h; the BSD sockets of
// Linux (sys/socket.h, netinet/in.h, arpa/inet.h) give htonl and the address functions.

#pragma once
#include <windows.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

/*
* Liam Ashdown
* Copyright (C) 2019
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Socket.hpp"

namespace SteerStone { namespace Policy { namespace Server {

    /// Policy Handler
    /// @p_ClientPacket : Packet recieved from client
    void PolicySocket::HandlePolicy(ClientPacket* p_Packet)
    {
        static const std::string l_Policy = "<?xml version=\"1.0\"?><cross-domain-policy xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://www.adobe.com/xml/schemas/PolicyFileSocket.xsd\"><allow-access-from domain=\"*\" to-ports=\"*\" secure=\"false\" /><site-control permitted-cross-domain-policies=\"master-only\" /></cross-domain-policy>";

        PacketBuffer l_PacketBuffer;
        l_PacketBuffer.AppendString(l_Policy);

        SendPacket(&l_PacketBuffer);
    }

}   ///< namespace Server
}   ///< namespace Policy
}   ///< namespace SteerStone
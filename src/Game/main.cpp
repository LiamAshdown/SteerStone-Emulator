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

#include "Network/Listener.hpp"
#include "Logger/BaseLogger.hpp"
#include "Config/Config.hpp"
#include "Database/DatabaseTypes.hpp"

#include "SessionSocket.hpp"
#include "Database/PreparedStatement.cpp"

DatabaseType GameDatabase;

int main()
{
    /// Log Enablers
    LOG_ENABLE_TIME(true);
    LOG_ENABLE_THREAD_ID(true);
    LOG_ENABLE_FUNCTION(true);
    SHOW_BANNER();

    if (!sConfig->SetFile("server.conf"))
        return -1;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    static std::string l_DatabaseInfo = sConfig->GetString("GameDatabaseInfo").c_str();
    static int32 l_Instances = sConfig->GetInt("MySQLInstances", 5);

    if (!GameDatabase.Start(l_DatabaseInfo.c_str(), l_Instances))
        return false;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    /// Network
    static const std::string l_Address          = sConfig->GetString("BindIP", "127.0.0.1");
    static const int32 l_Port                   = sConfig->GetInt("ServerPort", 37120);
    static const int32 l_NetworkChildProcesses  = sConfig->GetInt("ChildProcesses", 2);

    SteerStone::Core::Network::Listener<SteerStone::Game::Server::SessionSocket> l_Listener(l_Address, l_Port, l_NetworkChildProcesses);

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    while (true)
    {

    }

    system("pause");
}
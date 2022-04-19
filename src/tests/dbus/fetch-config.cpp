//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2022      Heiko Hund <heiko@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file   fetch-config.cpp
 *
 * @brief  Dumps a specific configuration stored in the configuration manager.
 *         This calls the D-Bus methods provided by the configuration manager
 *         directly and parses the results here.
 */

#include <iostream>
#include <dbus.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config obj path>" << std::endl;
        return 1;
    }

    DBus::asio::io_context ioc;
    auto dbus = DBus::Connection::create(ioc);
    if (!dbus)
    {
        std::cout << "** ERROR ** DBus::Connection::create()" << std::endl;
        return 2;
    }

    dbus->connect(
        DBus::Platform::getSystemBus(),
        DBus::AuthenticationProtocol::create(),
        [dbus, argv]
        (const DBus::Error& error, const std::string&, const std::string&)
        {
            if (error)
                throw std::runtime_error("** ERROR ** connect(): " + error.message + " (" + error.category + ")");

            dbus->getAllProperties(
                "net.openvpn.v3.configuration", // bus name
                argv[1],                        // object path
                "net.openvpn.v3.configuration", // interface name
                [dbus, argv]
                (const DBus::Error& error, const DBus::Message::MethodReturn& reply)
                {
                    if (error)
                        throw std::runtime_error("** ERROR ** getAllProperties(): " + error.message + " (" + error.category + ")");

                    struct {
                        bool valid;
                        bool readonly;
                        bool persistent;
                        bool single_use;
                        std::string name;
                    } props;

                    // reply contains an "ARRAY of DICT_ENTRY<STRING,VARIANT>"
                    for (const auto& elem : DBus::Type::refArray(reply.getParameter(0)))
                    {
                        const DBus::Type::DictEntry& dict_entry = DBus::Type::refDictEntry(elem);
                        auto key = DBus::Type::asString(dict_entry.key());
                        auto value = dict_entry.value();

                        if (key == "name")
                            props.name = DBus::Type::asString(value);
                        else if (key == "readonly")
                            props.readonly = DBus::Type::asBoolean(value);
                        else if (key == "persistent")
                            props.persistent = DBus::Type::asBoolean(value);
                        else if (key == "single_use")
                            props.single_use = DBus::Type::asBoolean(value);
                        else if (key == "valid")
                        {
                            props.valid = DBus::Type::asBoolean(value);
                            if (false == props.valid)
                                throw std::runtime_error("** ERROR ** Configuration is not valid");
                        }
                    }

                    dbus->sendMethodCall(
                      { "net.openvpn.v3.configuration",       // bus name
                        {
                          static_cast<const char *>(argv[1]), // object path
                          "net.openvpn.v3.configuration",     // interface name
                          "Fetch" } },                        // member name
                      [dbus, props]
                      (const DBus::Error& error, const DBus::Message::MethodReturn& reply)
                      {
                          if (error)
                              throw std::runtime_error( "** ERROR ** g_dbus_proxy_call_sync(): " + error.message + " (" + error.category + ")");

                          auto content = DBus::Type::asString(reply.getParameter(0));

                          std::cout << "Configuration:" << std::endl;
                          std::cout << "  - Name:       " << props.name << std::endl;
                          std::cout << "  - Read only:  " << (props.readonly ? "Yes" : "No") << std::endl;
                          std::cout << "  - Persistent: " << (props.persistent ? "Yes" : "No") << std::endl;
                          std::cout << "  - Usage:      " << (props.single_use ? "Once" : "Multiple times") << std::endl;
                          std::cout << "--------------------------------------------------" << std::endl;
                          std::cout << content << std::endl;
                          std::cout << "--------------------------------------------------" << std::endl;

                          dbus->disconnect();
                      });
                });
        });

    try
    {
        ioc.run();
    }
    catch (const std::runtime_error& error)
    {
        std::cout << error.what() << std::endl;
        dbus->disconnect();
        return 2;
    }

    std::cout << "** DONE" << std::endl;
}

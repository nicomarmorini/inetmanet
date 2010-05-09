//
// TraCIScenarioManagerLaunchd - TraCIScenarioManager that interacts with sumo-launchd
// Copyright (C) 2009 Christoph Sommer <christoph.sommer@informatik.uni-erlangen.de>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
 
#include "TraCIScenarioManagerLaunchd.h"
#include "TraCIConstants.h"
#define CMD_FILE_SEND 0x75
 
#include <sstream>
#include <iostream>
#include <fstream>
 
Define_Module(TraCIScenarioManagerLaunchd);
 
TraCIScenarioManagerLaunchd::~TraCIScenarioManagerLaunchd()
{
}
 
 
void TraCIScenarioManagerLaunchd::initialize()
{
  launchConfig = par("launchConfig").xmlValue();
  cXMLElementList basedir_nodes = launchConfig->getElementsByTagName("basedir");
  if (basedir_nodes.size() == 0) {
    // default basedir is where current network file was loaded from
    std::string basedir = cSimulation::getActiveSimulation()->getEnvir()->getConfig()->getConfigEntry("network").getBaseDirectory();
    cXMLElement* basedir_node = new cXMLElement("basedir", __FILE__, launchConfig);
    basedir_node->setAttribute("path", basedir.c_str());
    launchConfig->appendChild(basedir_node);
  }
  TraCIScenarioManager::initialize();
}
 
void TraCIScenarioManagerLaunchd::finish()
{
  TraCIScenarioManager::finish();
}
 
void TraCIScenarioManagerLaunchd::init_traci() {
 
  std::string contents = launchConfig->tostr(0);
 
  TraCIBuffer buf;
  buf << std::string("sumo-launchd.launch.xml") << contents;
  sendTraCIMessage(makeTraCICommand(CMD_FILE_SEND, buf));
 
  TraCIBuffer obuf(receiveTraCIMessage());
  uint8_t cmdLength; obuf >> cmdLength;
  uint8_t commandResp; obuf >> commandResp; if (commandResp != CMD_FILE_SEND) error("Expected response to command %d, but got one for command %d", CMD_FILE_SEND, commandResp);
  uint8_t result; obuf >> result;
  std::string description; obuf >> description;
  if (result != RTYPE_OK) {
    EV << "Warning: Received non-OK response from TraCI server to command " << CMD_FILE_SEND << ":" << description.c_str() << std::endl;
  }
 
  TraCIScenarioManager::init_traci();
}
 

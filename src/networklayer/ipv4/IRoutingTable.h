//
// Copyright (C) 2008 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_IROUTINGTABLE_H
#define __INET_IROUTINGTABLE_H

#include <vector>
#include <omnetpp.h>
#include "INETDefs.h"
#include "IPAddress.h"
#include "IPRoute.h"  // not strictly required, but most clients will need it anyway


/** Returned by IRoutingTable as the result of multicast routing */
struct MulticastRoute
{
    InterfaceEntry *interf;
    IPAddress gateway;
};
typedef std::vector<MulticastRoute> MulticastRoutes;


/**
 * A C++ interface to abstract the functionality of IRoutingTable.
 * Referring to IRoutingTable via this interface makes it possible to
 * transparently replace IRoutingTable with a different implementation,
 * without any change to the base INET.
 *
 * @see IRoutingTable, IPRoute
 */
class INET_API IRoutingTable
{
  public:
    virtual ~IRoutingTable() {};

    /**
     * For debugging
     */
    virtual void printRoutingTable() const = 0;

    /** @name Interfaces */
    //@{
    virtual void configureInterfaceForIPv4(InterfaceEntry *ie) = 0;

    /**
     * Returns an interface given by its address. Returns NULL if not found.
     */
    virtual InterfaceEntry *getInterfaceByAddress(const IPAddress& address) const = 0;
    //@}

    /**
     * IP forwarding on/off
     */
    virtual bool isIPForwardingEnabled() = 0;

    /**
     * Returns routerId.
     */
    virtual IPAddress getRouterId() = 0;

    /**
     * Sets routerId.
     */
    virtual void setRouterId(IPAddress a) = 0;

    /** @name Routing functions (query the route table) */
    //@{
    /**
     * Checks if the address is a local one, i.e. one of the host's.
     */
    virtual bool isLocalAddress(const IPAddress& dest) const = 0;
    /** @name Routing functions (query the route table) */
	//@{
	/**
	 * Checks if the address is a local broadcast one, i.e. one 192.168.0.255/24
	 */
	virtual bool isLocalBroadcastAddress(const IPAddress& dest) const = 0;

    /**
     * The routing function.
     */
    virtual const IPRoute *findBestMatchingRoute(const IPAddress& dest) const = 0;

    /**
     * Convenience function based on findBestMatchingRoute().
     *
     * Returns the interface Id to send the packets with dest as
     * destination address, or -1 if destination is not in routing table.
     */
    virtual InterfaceEntry *getInterfaceForDestAddr(const IPAddress& dest) const = 0;

    /**
     * Convenience function based on findBestMatchingRoute().
     *
     * Returns the gateway to send the destination. Returns null address
     * if the destination is not in routing table or there is
     * no gateway (local delivery).
     */
    virtual IPAddress getGatewayForDestAddr(const IPAddress& dest) const = 0;
    //@}

    /** @name Multicast routing functions */
    //@{

    /**
     * Checks if the address is in one of the local multicast group
     * address list.
     */
    virtual bool isLocalMulticastAddress(const IPAddress& dest) const = 0;

    /**
     * Returns routes for a multicast address.
     */
    virtual MulticastRoutes getMulticastRoutesFor(const IPAddress& dest) const = 0;
    //@}

    /** @name Route table manipulation */
    //@{

    /**
     * Returns the total number of routes (unicast, multicast, plus the
     * default route).
     */
    virtual int getNumRoutes() const = 0;

    /**
     * Returns the kth route. The returned route cannot be modified;
     * you must delete and re-add it instead. This rule is emphasized
     * by returning a const pointer.
     */
    virtual const IPRoute *getRoute(int k) const = 0;

    /**
     * Finds and returns the default route, or NULL if it doesn't exist
     */
    virtual const IPRoute *findRoute(const IPAddress& target, const IPAddress& netmask,
        const IPAddress& gw, int metric = 0, const char *dev = NULL) const = 0;
    

    virtual std::multimap<const IPRoute *, int> findRouteList(const IPAddress& target,
        const IPAddress& netmask, const IPAddress& gw, int metric = 0, const char *dev = NULL) = 0;

    /** Update route metric, returns 0 if no route was found and 1
     * otherwise
     */

    virtual int setMetric(int routeindex, int newmetric) = 0;

    /**
     * Finds and returns the default route, or NULL if it doesn't exist
     */
    virtual const IPRoute *getDefaultRoute() const = 0;

    /**
     * Adds a route to the routing table. Note that once added, routes
     * cannot be modified; you must delete and re-add them instead.
     */
    virtual void addRoute(const IPRoute *entry) = 0;

    /**
     * Deletes the given route from the routing table.
     * Returns true if the route was deleted correctly, false if it was
     * not in the routing table.
     */
    virtual bool deleteRoute(const IPRoute *entry) = 0;

    /**
     * Utility function: Returns a vector of all addresses of the node.
     */
    virtual std::vector<IPAddress> gatherAddresses() const = 0;
    //@}
   // Dsdv time to live test entry
    virtual void setTimeToLiveRoutingEntry(simtime_t a) = 0;
    virtual simtime_t getTimeToLiveRoutingEntry()=0;
    virtual void dsdvTestAndDelete() = 0;
    virtual const bool testValidity(const IPRoute *entry) const = 0;

    /* 
     * flush the whole routing table
     */
    virtual void flushTable() = 0;

    // Geo routing needs to update the value of metric value of all routes
    // to a node. It could delete all the routes that go to the node and
    // reinsert them but it would be extremely inefficent, that's why this
    // function has been added. Returns 0 if no route has been found for
    // the IP, route Metric is changed to rand(1, maxMetric)
    virtual bool scrambleRoutes(const IPAddress&, int maxMetric) = 0;

    // invalidates routing cache and local addresses cache
    virtual void invalidateCache() = 0;

};

#endif


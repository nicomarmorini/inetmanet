#include <omnetpp.h>
#include "CommonPartSublayerAuthorizationModule.h"


Define_Module(CommonPartSublayerAuthorizationModule);


CommonPartSublayerAuthorizationModule::CommonPartSublayerAuthorizationModule() {
}

CommonPartSublayerAuthorizationModule::~CommonPartSublayerAuthorizationModule() {

}

void CommonPartSublayerAuthorizationModule::initialize() {
	availableDatarate[ldUPLINK] = 0;
	availableDatarate[ldDOWNLINK] = 0;
}

void CommonPartSublayerAuthorizationModule::handleMessage( cMessage *msg ) {

}

bool CommonPartSublayerAuthorizationModule::checkQoSParams( sf_QoSParamSet *req_params, link_direction link_type ) {

	int max_sustained_traffic_rate = req_params->max_sustained_traffic_rate;
	int min_reserved_traffic_rate = req_params->min_reserved_traffic_rate;
	int max_latency = req_params->max_latency;
	int tolerated_jitter = req_params->tolerated_jitter;
	int priority = req_params->traffic_priority;
	req_tx_policy tx_policy = req_params->request_transmission_policy;

	if ( link_type == ldMANAGEMENT )
		error("Wrong link direction given! Must be either ldUPLINK or ldDOWNLINK!");

	switch ( getTypeOfServiceFlow(req_params) ) {
		case UGS:
			if ( availableDatarate[link_type] - max_sustained_traffic_rate >= 0 ) {
				availableDatarate[link_type] -= max_sustained_traffic_rate;
				return true;
			}
			else if ( availableDatarate[link_type] - min_reserved_traffic_rate >= 0 ) {
				availableDatarate[link_type] -= min_reserved_traffic_rate;
				return true;
			}
			else
				// TODO jemanden rausschmeissen, ansonsten:
				return false;
			break;

		case RTPS:
			return false;
			break;

		case NRTPS:
			return false;
			break;

		case BE:
			return false;
			break;
	}

	updateDisplay();
}

//void CommonPartSublayerAuthorizationModule::setServiceFlowMap( ServiceFlowMap *sf_map ) {
//	Enter_Method_Silent();
//
//	map_serviceFlows = sf_map;
//}

void CommonPartSublayerAuthorizationModule::setAvailableUplinkDatarate( int datarate ) {
	Enter_Method_Silent();

	availableDatarate[ldUPLINK] = datarate;
}

void CommonPartSublayerAuthorizationModule::setAvailableDownlinkDatarate( int datarate ) {
	Enter_Method_Silent();

	availableDatarate[ldDOWNLINK] = datarate;
}

ip_traffic_types CommonPartSublayerAuthorizationModule::getTypeOfServiceFlow( sf_QoSParamSet *req_params ) {
	int max_sustained_traffic_rate = req_params->max_sustained_traffic_rate;
	int min_reserved_traffic_rate = req_params->min_reserved_traffic_rate;
	int max_latency = req_params->max_latency;
	int tolerated_jitter = req_params->tolerated_jitter;
	int priority = req_params->traffic_priority;
	req_tx_policy tx_policy = req_params->request_transmission_policy;

	if ( max_sustained_traffic_rate > 0 &&
			min_reserved_traffic_rate == max_sustained_traffic_rate &&
			tolerated_jitter > 0 )
		return UGS;
	else if ( max_sustained_traffic_rate > 0 &&
				min_reserved_traffic_rate < max_sustained_traffic_rate &&
				max_latency > 0 )
		return RTPS;
//	else if ( max_sustained_traffic_rate > 0 &&
//				min_reserved_traffic_rate < max_sustained_traffic_rate &&
//				max_latency > 0 &&
//				tx_policy-> )
//		return ERTPS;
	else if ( min_reserved_traffic_rate > 0 &&
				max_sustained_traffic_rate == 0 &&
				priority > 0 )
		return NRTPS;
	else if ( min_reserved_traffic_rate == 0 &&
				max_sustained_traffic_rate == 0 &&
				max_latency == 0 )
		return BE;
}

void CommonPartSublayerAuthorizationModule::updateDisplay()
{
	char buf[50];
	sprintf(buf,"Available Downlink/s: %d \nAvailable Uplink/s: %d",
				availableDatarate[ldDOWNLINK],
				availableDatarate[ldUPLINK] );

    getDisplayString().setTagArg("t",0,buf);
    getDisplayString().setTagArg("t",1,"t");
}

package inet.underTest.linklayer.ieee802154.phyLayer;
simple Ieee802154Phy
{
    parameters:

        bool debug;
        double channelNumber = default(0);

        // power used for transmission of messages (mW)
        double transmitterPower @unit("mW") = default(1.0mW);

        // received signals with power below sensitivity are ignored (dBm)
        double sensitivity @unit("dBm")=default(-85dBm);

        // base noise level (dBm)
        double thermalNoise @unit("dBm") = default(-110dBm);

        // used by PathLossReceptionModel
        double pathLossAlpha = default(2);

        // used by Ieee802154RadioModel
        double snirThreshold @unit("dB") = default(4dB); 
        double carrierFrequency @unit("Hz") = default(2.4GHz); 

    gates:
        input uppergateIn;			// from Mac
        output uppergateOut;			// to Mac
        input radioIn; 					// to receive frames (AirFrame) on the radio channel
}

package inet.examples.wireless.WiMAXHandover;
//
// Roland Siedlaczek
//


import inet.examples.wireless.WiMAXHandover.Bild;
import inet.underTest.nodes.WiMAX.WiMAXMS;
import inet.underTest.nodes.WiMAX.WiMAXAP;
import inet.world.ChannelControl;

module HandoverNetworkFourBS
{
    parameters:
        double playgroundSizeX;
        double playgroundSizeY;
        @display("bgb=1085,582");
    submodules:
        ms2: WiMAXMS {
            parameters:
                @display("p=750,2000;i=device/cellphone_s;r=,,#707070");
        }
        bs1: WiMAXAP {
            parameters:
                @display("p=1000,2500;i=device/antennatower_l;r=,,#707070");
        }
        bs2: WiMAXAP {
            parameters:
                @display("p=2000,4000;i=device/antennatower_l;r=,,#707070");
        }
        bs3: WiMAXAP {
            parameters:
                @display("p=3000,2500;i=device/antennatower_l;r=,,#707070");
        }
        bs4: WiMAXAP {
            parameters:
                @display("p=2000,1000;i=device/antennatower_l;r=,,#707070");
        }
        channelcontrol: ChannelControl {
            parameters:
                playgroundSizeX = playgroundSizeX;
                playgroundSizeY = playgroundSizeY;
                @display("p=60,50;i=misc/sun");
        }
    connections allowunconnected:
}

//network HandoverNetworkFourBS extends HandoverNetworkFourBS
//{
//    parameters:
//}


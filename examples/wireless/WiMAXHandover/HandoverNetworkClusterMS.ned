package inet.examples.wireless.WiMAXHandover;
//
// Roland Siedlaczek
//


import inet.examples.wireless.WiMAXHandover.Bild;
import inet.underTest.nodes.WiMAX.WiMAXMS;
import inet.underTest.nodes.WiMAX.WiMAXAP;
import inet.world.ChannelControl;



module HandoverNetworkClusterMS
{
    parameters:
        double playgroundSizeX;
        double playgroundSizeY;
        @display("bgb=1000,1000");
    submodules:
        ms1: WiMAXMS {
            parameters:
                @display("p=300,300;i=device/cellphone_s;r=,,#707070");
        }
        ms2: WiMAXMS {
            parameters:
                @display("p=300,300;i=device/cellphone_s;r=,,#707070");
        }
        ms3: WiMAXMS {
            parameters:
                @display("p=300,300;i=device/cellphone_s;r=,,#707070");
        }
        ms4: WiMAXMS {
            parameters:
                @display("p=300,300;i=device/cellphone_s;r=,,#707070");
        }
        ms5: WiMAXMS {
            parameters:
                @display("p=300,300;i=device/cellphone_s;r=,,#707070");
        }
        ms6: WiMAXMS {
            parameters:
                @display("p=300,300;i=device/cellphone_s;r=,,#707070");
        }
        bs1: WiMAXAP {
            parameters:
                @display("p=300,300;i=device/antennatower_l;r=,,#707070");
        }
        bs2: WiMAXAP {
            parameters:
                @display("p=100,300;i=device/antennatower_l;r=,,#707070");
        }
        bs3: WiMAXAP {
            parameters:
                @display("p=200,473;i=device/antennatower_l;r=,,#707070");
        }
        bs4: WiMAXAP {
            parameters:
                @display("p=400,473;i=device/antennatower_l;r=,,#707070");
        }
        bs5: WiMAXAP {
            parameters:
                @display("p=500,300;i=device/antennatower_l;r=,,#707070");
        }
        bs6: WiMAXAP {
            parameters:
                @display("p=400,127;i=device/antennatower_l;r=,,#707070");
        }
        bs7: WiMAXAP {
            parameters:
                @display("p=200,127;i=device/antennatower_l;r=,,#707070");
        }
        channelcontrol: ChannelControl {
            parameters:
                playgroundSizeX = playgroundSizeX;
                playgroundSizeY = playgroundSizeY;
                @display("p=60,50;i=misc/sun");
        }
    connections allowunconnected:
}

//network HandoverNetworkClusterMS extends HandoverNetworkClusterMS
//{
//    parameters:
//}


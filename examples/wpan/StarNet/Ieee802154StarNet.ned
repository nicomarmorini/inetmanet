package inet.examples.wpan.StarNet;

import inet.underTest.nodes.wpan.Ieee802154Node;
import inet.world.ChannelControl;

network Ieee802154StarNet
{
    parameters:
        double numHosts;
        double playgroundSizeX;
        double playgroundSizeY;
    submodules:
        host[numHosts]: Ieee802154Node {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070");
        }
        channelcontrol: ChannelControl {
            parameters:
                playgroundSizeX = playgroundSizeX;
                playgroundSizeY = playgroundSizeY;
                @display("p=-65,20;i=misc/sun");
        }
    connections allowunconnected:
}


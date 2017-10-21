import QtQuick 2.0
import ShincoShell 1.0

Item {
    visible: true
    width: 1024
    height: 600

    property int vipIndex: 0

    function getVideoSource() {
        return "image://videoprovider/" + Number(vipIndex++)
    }

    Connections {
        target: mainwindow

        onImgProduced: {
            bufImg.source = img
        }

        onVideoUpdated: {
            img.source = getVideoSource()
        }
    }

    Image {
        id: img

        sourceSize.width: 720
        sourceSize.height: 480
    }

    BufImage {
        id: bufImg
        anchors.fill: parent

        sourceSize.width: parent.width
        sourceSize.height: parent.height
    }
}

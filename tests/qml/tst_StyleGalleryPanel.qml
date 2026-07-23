import QtQuick
import QtQuick.Controls
import QtTest

import GarmentStyleMatch

Item {
    width: 480
    height: 720

    Item {
        id: host
        anchors.fill: parent
    }

    TestCase {
        id: testCase

        name: "StyleGalleryPanel"
        when: windowShown

        Component {
            id: panelComponent

            StyleGalleryPanel {
                width: host.width
                height: host.height
                model: galleryTestModel
                currentPhotoSelected: true
            }
        }

        SignalSpy {
            id: matchSpy
            signalName: "matchRequested"
        }

        SignalSpy {
            id: confirmSpy
            signalName: "confirmRequested"
        }

        property var panel

        function child(name) {
            return findChild(panel, name);
        }

        function waitForDelegate() {
            tryVerify(function () {
                return child("galleryCell-1") !== null;
            });
        }

        function showHoverActions() {
            const area = child("galleryMouseArea-1");
            verify(area !== null);
            mouseMove(area, area.width / 2, area.height / 2);
            tryCompare(child("galleryActionGrid-1"), "visible", true);
        }

        function openMenu() {
            const area = child("galleryMouseArea-1");
            verify(area !== null);
            mouseClick(area, area.width / 2, area.height / 2, Qt.RightButton);
        }

        function init() {
            galleryTestModel.setPart("unknown");
            panel = createTemporaryObject(panelComponent, host);
            verify(panel !== null);
            matchSpy.target = panel;
            confirmSpy.target = panel;
            matchSpy.clear();
            confirmSpy.clear();
            waitForDelegate();
        }

        function cleanup() {
            matchSpy.target = null;
            confirmSpy.target = null;
            panel = null;
        }

        function test_actionVisibility_data() {
            return [
                {
                    tag: "upper",
                    part: "upper",
                    upper: true,
                    lower: false,
                    menu: true,
                    layoutHeight: 28
                },
                {
                    tag: "lower",
                    part: "lower",
                    upper: false,
                    lower: true,
                    menu: true,
                    layoutHeight: 28
                },
                {
                    tag: "accessory",
                    part: "accessory",
                    upper: false,
                    lower: false,
                    menu: false,
                    layoutHeight: 0
                },
                {
                    tag: "dress",
                    part: "dress",
                    upper: true,
                    lower: true,
                    menu: true,
                    layoutHeight: 60
                },
                {
                    tag: "unknown",
                    part: "unknown",
                    upper: true,
                    lower: true,
                    menu: true,
                    layoutHeight: 60
                }
            ];
        }

        function test_actionVisibility(data) {
            galleryTestModel.setPart(data.part);
            waitForDelegate();
            showHoverActions();

            compare(child("galleryMatchUpperButton-1").visible, data.upper);
            compare(child("galleryConfirmUpperButton-1").visible, data.upper);
            compare(child("galleryMatchLowerButton-1").visible, data.lower);
            compare(child("galleryConfirmLowerButton-1").visible, data.lower);
            compare(child("galleryActionGrid-1").implicitHeight, data.layoutHeight);

            openMenu();
            const menu = child("galleryMatchMenu");
            if (data.menu)
                tryCompare(menu, "visible", true);
            else
                compare(menu.visible, false);
            compare(child("galleryMatchUpperMenuItem").visible, data.upper);
            compare(child("galleryConfirmUpperMenuItem").visible, data.upper);
            compare(child("galleryMatchLowerMenuItem").visible, data.lower);
            compare(child("galleryConfirmLowerMenuItem").visible, data.lower);
            menu.close();
        }

        function test_retainedActionsKeepSignalContract() {
            galleryTestModel.setPart("upper");
            waitForDelegate();
            showHoverActions();
            mouseClick(child("galleryMatchUpperButton-1"));
            compare(matchSpy.count, 1);
            compare(matchSpy.signalArguments[0][0], 1);
            compare(matchSpy.signalArguments[0][1], "upper");

            galleryTestModel.setPart("lower");
            waitForDelegate();
            openMenu();
            tryCompare(child("galleryMatchMenu"), "visible", true);
            mouseClick(child("galleryConfirmLowerMenuItem"));
            compare(confirmSpy.count, 1);
            compare(confirmSpy.signalArguments[0][0], 1);
            compare(confirmSpy.signalArguments[0][1], "lower");
        }

        function test_currentPhotoSelectionStillControlsEnablement() {
            galleryTestModel.setPart("upper");
            waitForDelegate();
            panel.currentPhotoSelected = false;
            compare(child("galleryMatchUpperButton-1").enabled, false);
            compare(child("galleryConfirmUpperButton-1").enabled, false);

            panel.currentPhotoSelected = true;
            compare(child("galleryMatchUpperButton-1").enabled, true);
            compare(child("galleryConfirmUpperButton-1").enabled, true);
        }

        function test_modelRebuildInvalidatesOpenMenuAndUsesNewPart() {
            galleryTestModel.setPart("unknown");
            waitForDelegate();
            openMenu();
            tryCompare(child("galleryMatchMenu"), "visible", true);

            galleryTestModel.setPart("upper");
            tryCompare(child("galleryMatchMenu"), "visible", false);
            waitForDelegate();
            showHoverActions();
            compare(child("galleryMatchUpperButton-1").visible, true);
            compare(child("galleryMatchLowerButton-1").visible, false);
        }

        function test_actionlessContextClosesOpenMenu() {
            galleryTestModel.setPart("unknown");
            waitForDelegate();
            openMenu();
            const menu = child("galleryMatchMenu");
            tryCompare(menu, "visible", true);

            panel.openMatchMenu(0, "accessory");
            compare(menu.visible, false);
            compare(menu.galleryRow, 0);
            compare(menu.part, "accessory");
        }
    }
}

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

        SignalSpy {
            id: categoryOverrideSpy
            signalName: "categoryOverrideRequested"
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
            categoryOverrideSpy.target = panel;
            matchSpy.clear();
            confirmSpy.clear();
            categoryOverrideSpy.clear();
            waitForDelegate();
        }

        function cleanup() {
            matchSpy.target = null;
            confirmSpy.target = null;
            categoryOverrideSpy.target = null;
            panel = null;
        }

        function test_actionVisibility_data() {
            return [
                {
                    part: "upper",
                    label: "上衣",
                    upper: true,
                    lower: false,
                    dress: false,
                    menu: true,
                    layoutHeight: 28
                },
                {
                    part: "lower",
                    label: "裤裙",
                    upper: false,
                    lower: true,
                    dress: false,
                    menu: true,
                    layoutHeight: 28
                },
                {
                    part: "accessory",
                    label: "配件",
                    upper: false,
                    lower: false,
                    dress: false,
                    menu: true,
                    layoutHeight: 0
                },
                {
                    part: "dress",
                    label: "连衣裙",
                    upper: false,
                    lower: false,
                    dress: true,
                    menu: true,
                    layoutHeight: 28
                },
                {
                    part: "unknown",
                    label: "未知",
                    upper: true,
                    lower: true,
                    dress: false,
                    menu: true,
                    layoutHeight: 60
                }
            ];
        }

        function test_actionVisibility(data) {
            galleryTestModel.setPart(data.part);
            waitForDelegate();
            compare(child("galleryPartLabel-1").text, data.label);
            showHoverActions();

            compare(child("galleryMatchUpperButton-1").visible, data.upper);
            compare(child("galleryConfirmUpperButton-1").visible, data.upper);
            compare(child("galleryMatchLowerButton-1").visible, data.lower);
            compare(child("galleryConfirmLowerButton-1").visible, data.lower);
            compare(child("galleryMatchDressButton-1").visible, data.dress);
            compare(child("galleryConfirmDressButton-1").visible, data.dress);
            compare(child("galleryActionGrid-1").implicitHeight, data.layoutHeight);

            openMenu();
            const menu = child("galleryMatchMenu");
            if (data.menu)
                tryCompare(menu, "visible", true);
            else
                compare(menu.visible, false);
            compare(child("galleryMatchUpperMenuItem") !== null, data.upper);
            compare(child("galleryConfirmUpperMenuItem") !== null, data.upper);
            compare(child("galleryMatchLowerMenuItem") !== null, data.lower);
            compare(child("galleryConfirmLowerMenuItem") !== null, data.lower);
            compare(child("galleryMatchDressMenuItem") !== null, data.dress);
            compare(child("galleryConfirmDressMenuItem") !== null, data.dress);
            const expectedMenuItems = [];
            if (data.upper)
                expectedMenuItems.push("galleryMatchUpperMenuItem");
            if (data.lower)
                expectedMenuItems.push("galleryMatchLowerMenuItem");
            if (data.dress)
                expectedMenuItems.push("galleryMatchDressMenuItem");
            if (data.upper)
                expectedMenuItems.push("galleryConfirmUpperMenuItem");
            if (data.lower)
                expectedMenuItems.push("galleryConfirmLowerMenuItem");
            if (data.dress)
                expectedMenuItems.push("galleryConfirmDressMenuItem");
            compare(menu.count, expectedMenuItems.length + 1);
            for (let index = 0; index < expectedMenuItems.length; ++index)
                compare(menu.itemAt(index).objectName, expectedMenuItems[index]);
            compare(child("galleryCategoryOverrideMenu").count, 5);
            menu.close();
        }

        function test_partFilterOptionsAndSelection() {
            const box = child("galleryPartFilterBox");
            verify(box !== null);
            compare(box.count, 6);
            compare(box.textAt(0), "全部");
            compare(box.textAt(1), "上衣");
            compare(box.textAt(2), "裤裙");
            compare(box.textAt(3), "配件");
            compare(box.textAt(4), "连衣裙");
            compare(box.textAt(5), "未知");

            panel.categoryText = "lower";
            tryCompare(box, "currentIndex", 2);
            compare(box.currentText, "裤裙");
            compare(box.currentValue, "lower");

            panel.categoryText = "missing";
            tryCompare(box, "currentIndex", 0);
            compare(box.currentText, "全部");
        }

        function test_contextMenuCollapsesHiddenActions_data() {
            return [
                { tag: "no spacing", spacing: 0 },
                { tag: "compact spacing", spacing: 3 },
                { tag: "wide spacing", spacing: 11 }
            ];
        }

        function test_contextMenuCollapsesHiddenActions(data) {
            const menu = child("galleryMatchMenu");
            menu.contentItem.spacing = data.spacing;
            galleryTestModel.setPart("upper");
            waitForDelegate();
            openMenu();
            tryCompare(menu, "visible", true);
            const confirmUpperItem = child("galleryConfirmUpperMenuItem");
            tryVerify(function () {
                const confirmUpperBottom = confirmUpperItem.mapToItem(menu.contentItem, 0, confirmUpperItem.height).y;
                return confirmUpperBottom <= menu.contentItem.height;
            });
            const upperMenuHeight = menu.height;
            menu.close();

            galleryTestModel.setPart("unknown");
            waitForDelegate();
            openMenu();
            tryCompare(menu, "visible", true);
            tryVerify(function () { return menu.height > upperMenuHeight; });
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

            galleryTestModel.setPart("dress");
            waitForDelegate();
            showHoverActions();
            mouseClick(child("galleryMatchDressButton-1"));
            compare(matchSpy.count, 2);
            compare(matchSpy.signalArguments[1][0], 1);
            compare(matchSpy.signalArguments[1][1], "dress");

            openMenu();
            tryCompare(child("galleryMatchMenu"), "visible", true);
            mouseClick(child("galleryConfirmDressMenuItem"));
            compare(confirmSpy.count, 2);
            compare(confirmSpy.signalArguments[1][0], 1);
            compare(confirmSpy.signalArguments[1][1], "dress");
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

        function test_categoryOverrideAvailableWithoutCurrentPhoto() {
            galleryTestModel.setPart("upper");
            waitForDelegate();
            panel.currentPhotoSelected = false;
            openMenu();
            const menu = child("galleryMatchMenu");
            tryCompare(menu, "visible", true);
            compare(menu.count, 1);

            const categoryMenu = child("galleryCategoryOverrideMenu");
            compare(categoryMenu.count, 5);
            compare(categoryMenu.itemAt(0).objectName, "gallerySetCategory-upper");
            compare(categoryMenu.itemAt(1).objectName, "gallerySetCategory-lower");
            compare(categoryMenu.itemAt(2).objectName, "gallerySetCategory-dress");
            compare(categoryMenu.itemAt(3).objectName, "gallerySetCategory-accessory");
            compare(categoryMenu.itemAt(4).objectName, "gallerySetCategory-unknown");
            compare(child("gallerySetCategory-upper").checkable, false);
            compare(child("gallerySetCategory-lower").checkable, false);
            child("gallerySetCategory-lower").triggered();
            compare(categoryOverrideSpy.count, 1);
            compare(categoryOverrideSpy.signalArguments[0][0], 1);
            compare(categoryOverrideSpy.signalArguments[0][1], "lower");
        }

        function test_categoryRuleSelectionRestoresAndNeverStaysBlank() {
            const box = child("categoryRuleBox");
            verify(box !== null);

            panel.currentCategoryRule = "TeenieWeenie";
            panel.categoryRuleOptions = [
                { id: "", name: "不使用品类规则" },
                { id: "TeenieWeenie", name: "TeenieWeenie" }
            ];
            tryCompare(box, "currentIndex", 1);
            compare(box.currentText, "TeenieWeenie");

            panel.currentCategoryRule = "";
            tryCompare(box, "currentIndex", 0);
            compare(box.currentText, "不使用品类规则");

            panel.currentCategoryRule = "missing-rule";
            tryCompare(box, "currentIndex", 0);
            compare(box.currentText, "不使用品类规则");
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

        function test_accessoryContextStillOffersCategoryOverride() {
            galleryTestModel.setPart("unknown");
            waitForDelegate();
            openMenu();
            const menu = child("galleryMatchMenu");
            tryCompare(menu, "visible", true);

            panel.openMatchMenu(0, "accessory");
            tryCompare(menu, "visible", true);
            compare(menu.galleryRow, 0);
            compare(menu.part, "accessory");
            compare(child("gallerySetCategory-accessory").checkable, false);
        }
    }
}

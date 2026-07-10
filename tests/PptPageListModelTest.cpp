#include <QCoreApplication>
#include <QDebug>

#include "PptPageListModel.h"

namespace
{

    bool check(bool condition, const QString &message)
    {
        if (!condition)
            qCritical().noquote() << message;
        return condition;
    }

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    PptPageListModel model;

    model.setSelectedPagesText(QStringLiteral("3, 1"));
    if (!check(model.selectedPagesText() == QStringLiteral("1,3") && model.selectedCount() == 0,
               QStringLiteral("没有预览项时仍应保留并规范化页号文本")))
        return 1;

    model.appendItem({1, QStringLiteral("slide_1.png"), false});
    model.appendItem({2, QStringLiteral("slide_2.png"), false});
    model.appendItem({3, QStringLiteral("slide_3.png"), false});
    if (!check(model.selectedCount() == 2 && model.at(0)->selected && !model.at(1)->selected && model.at(2)->selected,
               QStringLiteral("预览项加载后应按已恢复页号自动勾选")))
        return 1;

    model.toggleSelected(0);
    model.toggleSelected(1);
    if (!check(model.selectedPagesText() == QStringLiteral("2,3"), QStringLiteral("点击页面后页号文本必须同步更新")))
        return 1;

    model.clear();
    model.setSelectedPagesText(QStringLiteral("4,6"));
    if (!check(model.selectedPagesText() == QStringLiteral("4,6"), QStringLiteral("清空预览后应能重新填入持久化页号")))
        return 1;

    return 0;
}

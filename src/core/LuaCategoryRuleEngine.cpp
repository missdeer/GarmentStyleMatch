// clang-format's project categories intentionally group extensionless standard headers before third-party .hpp headers.
// NOLINTBEGIN(llvm-include-order)
#include <algorithm>
#include <array>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <utility>
#include <lua.hpp>
// NOLINTEND(llvm-include-order)

#include "LuaCategoryRuleEngine.h"

namespace
{
    constexpr int      kInstructionHookInterval = 100;
    constexpr unsigned kLuaHashSeed             = 0x47534d;

    struct InstructionBudget
    {
        int  remaining           = 0;
        int *validationRemaining = nullptr;
        bool exhausted           = false;
    };

    struct AllocatorContext
    {
        std::size_t        used   = 0;
        std::size_t        limit  = 0;
        InstructionBudget *budget = nullptr;
    };

    void *limitedAllocator(void *userData, void *pointer, std::size_t oldSize, std::size_t newSize)
    {
        auto &context = *static_cast<AllocatorContext *>(userData);
        if (!pointer)
        {
            oldSize = 0;
        }
        if (newSize == 0)
        {
            std::free(pointer);
            context.used = oldSize > context.used ? 0 : context.used - oldSize;
            return nullptr;
        }

        const std::size_t growth = newSize > oldSize ? newSize - oldSize : 0;
        if (growth > context.limit - std::min(context.used, context.limit))
        {
            return nullptr;
        }

        void *resized = std::realloc(pointer, newSize);
        if (!resized)
        {
            return nullptr;
        }
        context.used = newSize >= oldSize ? context.used + growth : context.used - std::min(oldSize - newSize, context.used);
        return resized;
    }

    void instructionHook(lua_State *state, [[maybe_unused]] lua_Debug *debug)
    {
        void *userData = nullptr;
        lua_getallocf(state, &userData);
        auto              &context = *static_cast<AllocatorContext *>(userData);
        InstructionBudget *budget  = context.budget;
        if (!budget)
        {
            return;
        }

        budget->remaining -= kInstructionHookInterval;
        if (budget->validationRemaining)
        {
            *budget->validationRemaining -= kInstructionHookInterval;
        }
        if (budget->remaining <= 0 || (budget->validationRemaining && *budget->validationRemaining <= 0))
        {
            budget->exhausted = true;
            lua_pushliteral(state, "Lua instruction limit exceeded");
            lua_error(state);
        }
    }

    int openSafeLibraries(lua_State *state)
    {
        luaL_requiref(state, "_G", luaopen_base, 1);
        lua_pop(state, 1);
        luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
        lua_pop(state, 1);
        luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(state, 1);
        luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
        lua_pop(state, 1);

        constexpr std::array blockedGlobals = {
            "collectgarbage",
            "debug",
            "dofile",
            "getmetatable",
            "io",
            "load",
            "loadfile",
            "os",
            "package",
            "pcall",
            "print",
            "require",
            "setmetatable",
            "warn",
            "xpcall",
        };
        for (const char *name : blockedGlobals)
        {
            lua_pushnil(state);
            lua_setglobal(state, name);
        }
        return 0;
    }

    class StackGuard final
    {
    public:
        explicit StackGuard(lua_State *state) : m_state(state), m_top(lua_gettop(state)) {}
        ~StackGuard()
        {
            lua_settop(m_state, m_top);
        }

        StackGuard(const StackGuard &)            = delete;
        StackGuard &operator=(const StackGuard &) = delete;
        StackGuard(StackGuard &&)                 = delete;
        StackGuard &operator=(StackGuard &&)      = delete;

    private:
        lua_State *m_state;
        int        m_top;
    };

    void pushRawField(lua_State *state, int tableIndex, const char *name)
    {
        tableIndex = lua_absindex(state, tableIndex);
        lua_pushstring(state, name);
        lua_rawget(state, tableIndex);
    }

    std::optional<QString> stringField(lua_State *state, int tableIndex, const char *name)
    {
        pushRawField(state, tableIndex, name);
        if (lua_type(state, -1) != LUA_TSTRING)
        {
            lua_pop(state, 1);
            return std::nullopt;
        }
        std::size_t length = 0;
        const char *value  = lua_tolstring(state, -1, &length);
        QString     result = QString::fromUtf8(value, static_cast<qsizetype>(length));
        lua_pop(state, 1);
        return result;
    }

    std::optional<bool> boolField(lua_State *state, int tableIndex, const char *name)
    {
        pushRawField(state, tableIndex, name);
        if (lua_type(state, -1) != LUA_TBOOLEAN)
        {
            lua_pop(state, 1);
            return std::nullopt;
        }
        const bool result = lua_toboolean(state, -1) != 0;
        lua_pop(state, 1);
        return result;
    }

    std::optional<LuaCategoryRuleEngine::Part> partFromName(const QString &name)
    {
        using Part = LuaCategoryRuleEngine::Part;
        if (name == QLatin1String("upper"))
        {
            return Part::Upper;
        }
        if (name == QLatin1String("lower"))
        {
            return Part::Lower;
        }
        if (name == QLatin1String("accessory"))
        {
            return Part::Accessory;
        }
        if (name == QLatin1String("dress"))
        {
            return Part::Dress;
        }
        if (name == QLatin1String("unknown"))
        {
            return Part::Unknown;
        }
        return std::nullopt;
    }

    LuaCategoryRuleEngine::Result invalidResult(const QString &error)
    {
        LuaCategoryRuleEngine::Result result;
        result.error = error;
        return result;
    }

    LuaCategoryRuleEngine::Result parseResult(lua_State *state, int tableIndex)
    {
        if (lua_type(state, tableIndex) != LUA_TTABLE)
        {
            return invalidResult(QStringLiteral("Lua 分类结果必须是 table"));
        }
        tableIndex = lua_absindex(state, tableIndex);

        const auto recognized = boolField(state, tableIndex, "recognized");
        const auto partName   = stringField(state, tableIndex, "part");
        const auto part       = partName ? partFromName(*partName) : std::nullopt;
        if (!recognized || !part)
        {
            return invalidResult(QStringLiteral("Lua 分类结果缺少有效的 recognized 或 part"));
        }

        LuaCategoryRuleEngine::Result result;
        result.recognized = *recognized;
        result.part       = *part;
        pushRawField(state, tableIndex, "categoryCode");
        if (lua_type(state, -1) != LUA_TNIL)
        {
            if (lua_type(state, -1) != LUA_TSTRING)
            {
                lua_pop(state, 1);
                return invalidResult(QStringLiteral("Lua 分类结果的 categoryCode 必须是非空字符串"));
            }
            std::size_t length  = 0;
            const char *value   = lua_tolstring(state, -1, &length);
            result.categoryCode = QString::fromUtf8(value, static_cast<qsizetype>(length));
            if (result.categoryCode.isEmpty())
            {
                lua_pop(state, 1);
                return invalidResult(QStringLiteral("Lua 分类结果的 categoryCode 必须是非空字符串"));
            }
        }
        lua_pop(state, 1);
        if (!result.recognized)
        {
            if (result.part != LuaCategoryRuleEngine::Part::Unknown)
            {
                return invalidResult(QStringLiteral("未识别的 Lua 分类结果必须使用 unknown"));
            }
            return result;
        }
        if (result.part == LuaCategoryRuleEngine::Part::Unknown)
        {
            return invalidResult(QStringLiteral("已识别的 Lua 分类结果不能使用 unknown"));
        }

        const auto level1Code = stringField(state, tableIndex, "level1Code");
        const auto level1Name = stringField(state, tableIndex, "level1Name");
        const auto level2Code = stringField(state, tableIndex, "level2Code");
        const auto level2Name = stringField(state, tableIndex, "level2Name");
        if (result.categoryCode.isEmpty() || !level1Code || level1Code->isEmpty() || !level1Name || level1Name->isEmpty() || !level2Code ||
            level2Code->isEmpty() || !level2Name || level2Name->isEmpty())
        {
            return invalidResult(QStringLiteral("已识别的 Lua 分类结果缺少品类层级信息"));
        }

        result.level1Code = *level1Code;
        result.level1Name = *level1Name;
        result.level2Code = *level2Code;
        result.level2Name = *level2Name;
        return result;
    }

    bool equivalent(const LuaCategoryRuleEngine::Result &left, const LuaCategoryRuleEngine::Result &right)
    {
        return left.recognized == right.recognized && left.categoryCode == right.categoryCode && left.level1Code == right.level1Code &&
               left.level1Name == right.level1Name && left.level2Code == right.level2Code && left.level2Name == right.level2Name &&
               left.part == right.part;
    }

    QString luaError(lua_State *state, int status)
    {
        if (status == LUA_ERRMEM)
        {
            return QStringLiteral("Lua 内存上限已超出");
        }
        if (lua_type(state, -1) == LUA_TSTRING)
        {
            std::size_t length = 0;
            const char *value  = lua_tolstring(state, -1, &length);
            return QString::fromUtf8(value, static_cast<qsizetype>(length));
        }
        return QStringLiteral("Lua 执行失败（状态 %1）").arg(status);
    }
} // namespace

class LuaCategoryRuleEngine::Impl final
{
public:
    Impl(const QByteArray &script, Limits requestedLimits) : limits(requestedLimits), source(script)
    {
        if (limits.memoryBytes == 0 || limits.instructionsPerExecution <= 0 || limits.instructionsPerValidation <= 0)
        {
            loadError = QStringLiteral("Lua 资源上限必须大于 0");
            return;
        }

        allocator.limit = limits.memoryBytes;
#if LUA_VERSION_NUM >= 505
        lua = lua_newstate(limitedAllocator, &allocator, kLuaHashSeed);
#else
        lua = lua_newstate(limitedAllocator, &allocator);
#endif
        if (!lua)
        {
            loadError = QStringLiteral("无法在内存上限内创建 Lua 状态");
            return;
        }

        lua_pushcfunction(lua, openSafeLibraries);
        const int librariesStatus = lua_pcall(lua, 0, 0, 0);
        if (librariesStatus != LUA_OK)
        {
            loadError = QStringLiteral("无法初始化受限 Lua 标准库：%1").arg(luaError(lua, librariesStatus));
            lua_pop(lua, 1);
            return;
        }
        validateAndLoad();
    }

    ~Impl()
    {
        if (lua)
        {
            lua_close(lua);
        }
    }

    Impl(const Impl &)            = delete;
    Impl &operator=(const Impl &) = delete;
    Impl(Impl &&)                 = delete;
    Impl &operator=(Impl &&)      = delete;

    bool protectedCall(int arguments, int results, int *validationRemaining, QString *error)
    {
        InstructionBudget budget {limits.instructionsPerExecution, validationRemaining, false};
        allocator.budget = &budget;
        lua_sethook(lua, instructionHook, LUA_MASKCOUNT, kInstructionHookInterval);
        const int status = lua_pcall(lua, arguments, results, 0);
        lua_sethook(lua, nullptr, 0, 0);
        allocator.budget = nullptr;

        if (status == LUA_OK && !budget.exhausted)
        {
            return true;
        }
        if (error)
        {
            *error = budget.exhausted ? QStringLiteral("Lua 指令上限已超出") : luaError(lua, status);
        }
        if (status != LUA_OK)
        {
            lua_pop(lua, 1);
        }
        return false;
    }

    Result callClassify(const QString &normalizedStyleId, int *validationRemaining)
    {
        StackGuard guard(lua);
        lua_rawgeti(lua, LUA_REGISTRYINDEX, ruleReference);
        pushRawField(lua, -1, "classify");
        lua_remove(lua, -2);
        if (lua_type(lua, -1) != LUA_TFUNCTION)
        {
            return invalidResult(QStringLiteral("Lua 规则缺少 classify 函数"));
        }

        const QByteArray encoded = normalizedStyleId.toUtf8();
        lua_pushlstring(lua, encoded.constData(), static_cast<std::size_t>(encoded.size()));
        QString error;
        if (!protectedCall(1, 1, validationRemaining, &error))
        {
            return invalidResult(error);
        }
        return parseResult(lua, -1);
    }

    void validateAndLoad()
    {
        if (source.trimmed().isEmpty())
        {
            loadError = QStringLiteral("Lua 规则内容为空");
            return;
        }

        int       validationRemaining = limits.instructionsPerValidation;
        const int loadStatus          = luaL_loadbufferx(lua, source.constData(), static_cast<std::size_t>(source.size()), "category-rule", "t");
        if (loadStatus != LUA_OK)
        {
            loadError = QStringLiteral("Lua 规则语法错误：%1").arg(luaError(lua, loadStatus));
            lua_pop(lua, 1);
            return;
        }

        QString executionError;
        if (!protectedCall(0, 1, &validationRemaining, &executionError))
        {
            loadError = QStringLiteral("Lua 规则加载失败：%1").arg(executionError);
            return;
        }
        if (lua_type(lua, -1) != LUA_TTABLE)
        {
            loadError = QStringLiteral("Lua 规则必须返回 table");
            lua_pop(lua, 1);
            return;
        }

        ruleReference = luaL_ref(lua, LUA_REGISTRYINDEX);
        lua_rawgeti(lua, LUA_REGISTRYINDEX, ruleReference);
        const auto ruleId  = stringField(lua, -1, "ruleId");
        const auto version = stringField(lua, -1, "version");
        pushRawField(lua, -1, "classify");
        const bool hasClassify = lua_type(lua, -1) == LUA_TFUNCTION;
        lua_pop(lua, 1);
        pushRawField(lua, -1, "tests");
        const bool        hasTests  = lua_type(lua, -1) == LUA_TTABLE && lua_rawlen(lua, -1) > 0;
        const std::size_t testCount = hasTests ? lua_rawlen(lua, -1) : 0;
        lua_pop(lua, 2);

        if (!ruleId || ruleId->isEmpty() || !version || version->isEmpty() || !hasClassify || !hasTests)
        {
            loadError = QStringLiteral("Lua 规则必须声明 ruleId、version、classify 和非空 tests");
            return;
        }
        loadedRuleId      = *ruleId;
        loadedRuleVersion = *version;

        for (std::size_t index = 1; index <= testCount; ++index)
        {
            QString input;
            Result  expected;
            {
                StackGuard guard(lua);
                lua_rawgeti(lua, LUA_REGISTRYINDEX, ruleReference);
                pushRawField(lua, -1, "tests");
                lua_rawgeti(lua, -1, static_cast<lua_Integer>(index));
                if (lua_type(lua, -1) != LUA_TTABLE)
                {
                    loadError = QStringLiteral("Lua 自验证样例 %1 必须是 table").arg(index);
                    return;
                }
                const auto testInput = stringField(lua, -1, "input");
                pushRawField(lua, -1, "expected");
                if (!testInput)
                {
                    loadError = QStringLiteral("Lua 自验证样例 %1 缺少 input").arg(index);
                    return;
                }
                expected = parseResult(lua, -1);
                if (!expected.error.isEmpty())
                {
                    loadError = QStringLiteral("Lua 自验证样例 %1 的预期结果无效：%2").arg(index).arg(expected.error);
                    return;
                }
                input = *testInput;
            }

            const Result actual = callClassify(input, &validationRemaining);
            if (!actual.error.isEmpty() || !equivalent(actual, expected))
            {
                loadError = QStringLiteral("Lua 自验证样例 %1 失败%2")
                                .arg(index)
                                .arg(actual.error.isEmpty() ? QString() : QStringLiteral("：%1").arg(actual.error));
                return;
            }
        }

        ready = true;
        loadError.clear();
    }

    Limits           limits;
    QByteArray       source;
    AllocatorContext allocator;
    lua_State       *lua           = nullptr;
    int              ruleReference = LUA_NOREF;
    bool             ready         = false;
    QString          loadError;
    QString          loadedRuleId;
    QString          loadedRuleVersion;
    std::mutex       mutex;
};

LuaCategoryRuleEngine::LuaCategoryRuleEngine(const QByteArray &script) : LuaCategoryRuleEngine(script, Limits {}) {}

LuaCategoryRuleEngine::LuaCategoryRuleEngine(const QByteArray &script, Limits limits) : m_impl(std::make_unique<Impl>(script, limits)) {}

LuaCategoryRuleEngine::~LuaCategoryRuleEngine() = default;

LuaCategoryRuleEngine::State LuaCategoryRuleEngine::state() const
{
    return m_impl->ready ? State::Ready : State::Rejected;
}

QString LuaCategoryRuleEngine::errorMessage() const
{
    return m_impl->loadError;
}

QString LuaCategoryRuleEngine::ruleId() const
{
    return m_impl->loadedRuleId;
}

QString LuaCategoryRuleEngine::ruleVersion() const
{
    return m_impl->loadedRuleVersion;
}

LuaCategoryRuleEngine::Result LuaCategoryRuleEngine::classify(const QString &normalizedStyleId) const
{
    const std::scoped_lock lock(m_impl->mutex);
    if (!m_impl->ready)
    {
        return invalidResult(m_impl->loadError.isEmpty() ? QStringLiteral("Lua 规则不可用") : m_impl->loadError);
    }
    return m_impl->callClassify(normalizedStyleId, nullptr);
}

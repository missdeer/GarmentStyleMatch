local categories = {
    ["down-jacket"] = {
        level1Code = "2", level1Name = "外套", level2Code = "2.5", level2Name = "羽绒", part = "upper"
    },
    ["padded-jacket"] = {
        level1Code = "2", level1Name = "外套", level2Code = "2.6", level2Name = "棉服", part = "upper"
    },
    jacket = {
        level1Code = "2", level1Name = "外套", level2Code = "2.1", level2Name = "夹克", part = "upper"
    },
    ["denim-jacket"] = {
        level1Code = "2", level1Name = "外套", level2Code = "2.8", level2Name = "牛仔外套", part = "upper"
    },
    ["wool-coat"] = {
        level1Code = "2", level1Name = "外套", level2Code = "2.7", level2Name = "毛呢", part = "upper"
    },
    ["leather-jacket"] = {
        level1Code = "2", level1Name = "外套", level2Code = "2.9", level2Name = "皮衣", part = "upper"
    },
    suit = {
        level1Code = "2", level1Name = "外套", level2Code = "2.2", level2Name = "西装", part = "upper"
    },
    trench = {
        level1Code = "2", level1Name = "外套", level2Code = "2.3", level2Name = "风衣", part = "upper"
    },
    vest = {
        level1Code = "2", level1Name = "外套", level2Code = "2.4", level2Name = "背心", part = "upper"
    },
    knitwear = {
        level1Code = "1", level1Name = "上衣", level2Code = "1.5", level2Name = "毛针织", part = "upper"
    },
    sweatshirt = {
        level1Code = "1", level1Name = "上衣", level2Code = "1.2", level2Name = "卫衣", part = "upper"
    },
    tshirt = {
        level1Code = "1", level1Name = "上衣", level2Code = "1.1", level2Name = "T恤", part = "upper"
    },
    polo = {
        level1Code = "1", level1Name = "上衣", level2Code = "1.4", level2Name = "POLO", part = "upper"
    },
    shirt = {
        level1Code = "1", level1Name = "上衣", level2Code = "1.3", level2Name = "衬衫", part = "upper"
    },
    sweatpants = {
        level1Code = "3", level1Name = "裤子", level2Code = "3.1", level2Name = "卫裤", part = "lower"
    },
    trousers = {
        level1Code = "3", level1Name = "裤子", level2Code = "3.2", level2Name = "休闲裤", part = "lower"
    },
    jeans = {
        level1Code = "3", level1Name = "裤子", level2Code = "3.3", level2Name = "牛仔裤", part = "lower"
    },
    ["suit-trousers"] = {
        level1Code = "3", level1Name = "裤子", level2Code = "3.4", level2Name = "西装裤", part = "lower"
    },
    skirt = {
        level1Code = "3", level1Name = "裤子", level2Code = "3.5", level2Name = "半身裙", part = "lower"
    },
    accessory = {
        level1Code = "4", level1Name = "配件", level2Code = "4.1", level2Name = "配饰", part = "accessory"
    }
}

local codeToCategory = {
    JD = "down-jacket",
    JP = "padded-jacket",
    JJ = "jacket",
    JE = "denim-jacket",
    JW = "wool-coat",
    JL = "leather-jacket",
    JK = "suit",
    JT = "trench",
    VW = "vest",
    CK = "knitwear", KW = "knitwear", KN = "knitwear",
    MZ = "sweatshirt", MW = "sweatshirt", MA = "sweatshirt", MH = "sweatshirt",
    LW = "tshirt", LS = "tshirt", LA = "tshirt", RW = "tshirt",
    RS = "tshirt", RA = "tshirt", RN = "tshirt", RL = "tshirt",
    HW = "polo", HS = "polo", HA = "polo",
    YW = "shirt", YS = "shirt", YC = "shirt", YP = "shirt", YA = "shirt",
    TM = "sweatpants", MT = "sweatpants",
    TC = "trousers", TH = "trousers", TG = "trousers",
    TJ = "jeans", TF = "jeans",
    TW = "suit-trousers",
    WH = "skirt",
    AY = "accessory", AC = "accessory", AN = "accessory", AK = "accessory",
    XP = "accessory", AW = "accessory", AM = "accessory", AB = "accessory",
    AF = "accessory", AG = "accessory", AP = "accessory", AS = "accessory",
    AX = "accessory", FD = "accessory", FT = "accessory", MS = "accessory",
    OA = "accessory", PP = "accessory"
}

local function unknown(code)
    local result = { recognized = false, part = "unknown" }
    if code ~= nil then
        result.categoryCode = code
    end
    return result
end

local function classify(normalizedStyleId)
    if #normalizedStyleId < 4 then
        return unknown()
    end

    local code = normalizedStyleId:sub(3, 4)
    local category = categories[codeToCategory[code]]
    if category == nil then
        return unknown(code)
    end

    return {
        recognized = true,
        categoryCode = code,
        level1Code = category.level1Code,
        level1Name = category.level1Name,
        level2Code = category.level2Code,
        level2Name = category.level2Name,
        part = category.part
    }
end

local expectedGroups = {
    { codes = { "JD" }, level1Code = "2", level1Name = "外套", level2Code = "2.5", level2Name = "羽绒", part = "upper" },
    { codes = { "JP" }, level1Code = "2", level1Name = "外套", level2Code = "2.6", level2Name = "棉服", part = "upper" },
    { codes = { "JJ" }, level1Code = "2", level1Name = "外套", level2Code = "2.1", level2Name = "夹克", part = "upper" },
    { codes = { "JE" }, level1Code = "2", level1Name = "外套", level2Code = "2.8", level2Name = "牛仔外套", part = "upper" },
    { codes = { "JW" }, level1Code = "2", level1Name = "外套", level2Code = "2.7", level2Name = "毛呢", part = "upper" },
    { codes = { "JL" }, level1Code = "2", level1Name = "外套", level2Code = "2.9", level2Name = "皮衣", part = "upper" },
    { codes = { "JK" }, level1Code = "2", level1Name = "外套", level2Code = "2.2", level2Name = "西装", part = "upper" },
    { codes = { "JT" }, level1Code = "2", level1Name = "外套", level2Code = "2.3", level2Name = "风衣", part = "upper" },
    { codes = { "VW" }, level1Code = "2", level1Name = "外套", level2Code = "2.4", level2Name = "背心", part = "upper" },
    { codes = { "CK", "KW", "KN" }, level1Code = "1", level1Name = "上衣", level2Code = "1.5", level2Name = "毛针织", part = "upper" },
    { codes = { "MZ", "MW", "MA", "MH" }, level1Code = "1", level1Name = "上衣", level2Code = "1.2", level2Name = "卫衣", part = "upper" },
    { codes = { "LW", "LS", "LA", "RW", "RS", "RA", "RN", "RL" }, level1Code = "1", level1Name = "上衣", level2Code = "1.1", level2Name = "T恤", part = "upper" },
    { codes = { "HW", "HS", "HA" }, level1Code = "1", level1Name = "上衣", level2Code = "1.4", level2Name = "POLO", part = "upper" },
    { codes = { "YW", "YS", "YC", "YP", "YA" }, level1Code = "1", level1Name = "上衣", level2Code = "1.3", level2Name = "衬衫", part = "upper" },
    { codes = { "TM", "MT" }, level1Code = "3", level1Name = "裤子", level2Code = "3.1", level2Name = "卫裤", part = "lower" },
    { codes = { "TC", "TH", "TG" }, level1Code = "3", level1Name = "裤子", level2Code = "3.2", level2Name = "休闲裤", part = "lower" },
    { codes = { "TJ", "TF" }, level1Code = "3", level1Name = "裤子", level2Code = "3.3", level2Name = "牛仔裤", part = "lower" },
    { codes = { "TW" }, level1Code = "3", level1Name = "裤子", level2Code = "3.4", level2Name = "西装裤", part = "lower" },
    { codes = { "WH" }, level1Code = "3", level1Name = "裤子", level2Code = "3.5", level2Name = "半身裙", part = "lower" },
    { codes = { "AY", "AC", "AN", "AK", "XP", "AW", "AM", "AB", "AF", "AG", "AP", "AS", "AX", "FD", "FT", "MS", "OA", "PP" },
      level1Code = "4", level1Name = "配件", level2Code = "4.1", level2Name = "配饰", part = "accessory" }
}

local tests = {}
for _, group in ipairs(expectedGroups) do
    for _, code in ipairs(group.codes) do
        tests[#tests + 1] = {
            input = "T0" .. code .. "26B38A008",
            expected = {
                recognized = true,
                categoryCode = code,
                level1Code = group.level1Code,
                level1Name = group.level1Name,
                level2Code = group.level2Code,
                level2Name = group.level2Name,
                part = group.part
            }
        }
    end
end
tests[#tests + 1] = { input = "T0J", expected = unknown() }
tests[#tests + 1] = { input = "T0ZZ26B38A008", expected = unknown("ZZ") }

local mappingCount = 0
for _ in pairs(codeToCategory) do
    mappingCount = mappingCount + 1
end
assert(mappingCount == 59 and #tests == 61)

return {
    ruleId = "current-brand",
    version = "1",
    classify = classify,
    tests = tests
}

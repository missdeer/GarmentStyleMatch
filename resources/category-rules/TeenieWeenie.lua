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
    shorts = {
        level1Code = "3", level1Name = "裤子", level2Code = "3.5", level2Name = "短裤", part = "lower"
    },
    skirt = {
        level1Code = "3", level1Name = "裤子", level2Code = "3.6", level2Name = "半裙", part = "lower"
    },
    accessory = {
        level1Code = "4", level1Name = "配件", level2Code = "4.1", level2Name = "配饰", part = "accessory"
    },
    gift = {
        level1Code = "5", level1Name = "赠品", level2Code = "5.1", level2Name = "赠品", part = "accessory"
    }
}

local codeToCategory = {
    JD = "down-jacket", DM = "down-jacket", DL = "down-jacket",
    DS = "down-jacket", DK = "down-jacket", DV = "down-jacket",
    JP = "padded-jacket",
    JJ = "jacket", JA = "jacket",
    JE = "denim-jacket",
    CL = "wool-coat", CS = "wool-coat", CM = "wool-coat",
    CD = "wool-coat", JW = "wool-coat", JU = "wool-coat",
    JL = "leather-jacket",
    JK = "suit", JM = "suit",
    JT = "trench",
    VA = "vest", VJ = "vest", VL = "vest", VM = "vest", VW = "vest",
    CK = "knitwear", KW = "knitwear", KN = "knitwear", KC = "knitwear",
    KV = "knitwear", KA = "knitwear", KK = "knitwear",
    MZ = "sweatshirt", MW = "sweatshirt", MA = "sweatshirt", MH = "sweatshirt", MT = "sweatshirt",
    LW = "tshirt", LS = "tshirt", LA = "tshirt", RW = "tshirt",
    RS = "tshirt", RA = "tshirt", RN = "tshirt", RL = "tshirt",
    LB = "tshirt", RB = "tshirt", HB = "tshirt", HN = "tshirt",
    SN = "tshirt", SH = "tshirt", SR = "tshirt",
    HW = "polo", HS = "polo", HA = "polo",
    YW = "shirt", YS = "shirt", YC = "shirt", YP = "shirt", YA = "shirt",
    YB = "shirt", BL = "shirt", BN = "shirt", BW = "shirt", YN = "shirt",
    TM = "sweatpants",
    TC = "trousers", TG = "trousers", TA = "trousers",
    TK = "trousers", TN = "trousers", TV = "trousers",
    TH = "shorts", TF = "shorts",
    TJ = "jeans",
    TW = "suit-trousers",
    WH = "skirt",
    AY = "accessory", AC = "accessory", AN = "accessory", AK = "accessory",
    AW = "accessory", AM = "accessory", AB = "accessory",
    AF = "accessory", AG = "accessory", AP = "accessory", AS = "accessory",
    AX = "accessory", FD = "accessory", FT = "accessory", MS = "accessory",
    OA = "accessory", PP = "accessory", GB = "accessory", GS = "accessory",
    GD = "accessory", BG = "accessory", PG = "accessory", NB = "accessory",
    LN = "accessory", FG = "accessory", FJ = "accessory", SX = "accessory",
    FF = "accessory", QA = "accessory", WS = "accessory", AQ = "accessory",
    KH = "accessory", XP = "accessory",
    ZP = "gift"
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

return {
    ruleId = "TeenieWeenie",
    version = "2",
    classify = classify
}

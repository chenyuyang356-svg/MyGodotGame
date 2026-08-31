-- Aseprite Script: Team Color Alpha Mask V2
local sprite = app.activeSprite
if not sprite then
    app.alert("没有打开的图像！")
    return
end

if sprite.colorMode ~= ColorMode.RGB then
    app.alert("请先将图像转为 RGB 模式！")
    return
end

local dlg = Dialog("队伍颜色 Alpha 遮罩器 V2")

dlg:slider{ id="hueMin", label="色相下限 (Hue Min):", min=0, max=360, value=50 }
dlg:slider{ id="hueMax", label="色相上限 (Hue Max):", min=0, max=360, value=160 }
dlg:slider{ id="satMin", label="最低饱和度 (Sat Min):", min=0, max=100, value=10 }

dlg:separator()
dlg:label{ text="【升级版】支持编组图层遍历，且容忍微小透明度误差" }

dlg:button{ id="apply", text="应用 (Apply)", onclick=function()
    local data = dlg.data
    local hMin = data.hueMin
    local hMax = data.hueMax
    local sMin = data.satMin / 100.0
    local modifiedCount = 0

    -- 递归函数：用于钻进所有的图层编组（文件夹）
    local function processLayer(layer, frame)
        if layer.isGroup then
            for _, subLayer in ipairs(layer.layers) do
                processLayer(subLayer, frame)
            end
        elseif layer.isVisible and not layer.isReference then
            local cel = layer:cel(frame)
            if cel then
                local img = cel.image:clone()
                local changed = false
                
                for it in img:pixels() do
                    local pixelValue = it()
                    local a = app.pixelColor.rgbaA(pixelValue)

                    -- 容忍一点点 Alpha 误差 (>= 250 的都当做不透明实体处理)
                    if a >= 250 then
                        local r = app.pixelColor.rgbaR(pixelValue)
                        local g = app.pixelColor.rgbaG(pixelValue)
                        local b = app.pixelColor.rgbaB(pixelValue)

                        local c = Color{ r=r, g=g, b=b, a=a }
                        local h = c.hsvHue
                        local s = c.hsvSaturation

                        if h >= hMin and h <= hMax and s >= sMin then
                            -- 如果已经是 254 就跳过，不是的话才修改
                            if a ~= 254 then
                                it(app.pixelColor.rgba(r, g, b, 254))
                                changed = true
                                modifiedCount = modifiedCount + 1
                            end
                        end
                    end
                end
                
                if changed then
                    cel.image = img
                end
            end
        end
    end

    app.transaction(function()
        for _, frame in ipairs(sprite.frames) do
            for _, layer in ipairs(sprite.layers) do
                processLayer(layer, frame)
            end
        end
    end)
    
    app.alert("处理完成！共修改了 " .. modifiedCount .. " 个像素的 Alpha 值。")
    dlg:close()
end}

dlg:show()
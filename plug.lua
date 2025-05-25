local driver = {}

driver.north = function(input, size)
    return input:reverse()
end

driver.south = function(input, size)
    local out = {}
    for i = 1, size do
        local byte = input:byte(i)
        out[i] = string.char((byte + 1) % 256)
    end
    return table.concat(out)
end

function register_plugin(plugin)
    plugin.north_transform = driver.north
    plugin.south_transform = driver.south
    return plugin
end
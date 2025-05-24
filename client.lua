local client = require("e_device")
local socket = require("socket")

local c = client.create("lua5client", client.default_south_url, client.default_north_url, "demo")

c:set_callback(function(topic, msg)
    print("Received topic:", topic)
    print("Message:", msg)
end)

c:listen()
socket.sleep(1)

local i = 0
while true do
    local msg = string.format("ping from lua 5.1 %d", i)
    print("Sending message:", msg)
    c:send(msg)
    -- if i > 10 then
    --     break
    -- end
    i = i + 1
    socket.sleep(5)
end

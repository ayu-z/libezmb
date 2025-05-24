local e_device = require("e_device")
local socket = require("socket")

local m = e_device.create_collector("port3")

print("uid:", m.uid)
print("type:", m.type)
print("south_url:", m.south_url)
print("north_url:", m.north_url)

m:set_callback(function(topic, msg)
    print("[Collector] topic:", topic, "msg:", msg, "\n")
end)

m:listen()
print("Collector ready")

while true do
    m:send("[Collector] hello!!!\n")
    socket.sleep(1)
end



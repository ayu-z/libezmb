local e_device = require("e_device")
local socket = require("socket")

local m = e_device.create_monitor("port3")


print("uid:", m.uid)
print("type:", m.type)
print("south_url:", m.south_url)
print("north_url:", m.north_url)

m:set_callback(function(topic, msg)
    print("[Monitor] topic:", topic, "msg:", msg, "\n")
    m:send("[Monitor] hello!!!\n")
end)

m:listen()
print("Monitor ready")

while true do
    socket.sleep(1)
end

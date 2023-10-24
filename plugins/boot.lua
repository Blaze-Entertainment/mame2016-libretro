-- license:BSD-3-Clause
-- copyright-holders:Miodrag Milanovic
require('lfs')

local plugins_path = emu.plugins_path()
if (plugins_path == nil or plugins_path == '') then
	plugins_path = lfs.currentdir() .. "/plugins"
end
package.path = plugins_path .. "/?.lua;" .. plugins_path .. "/?/init.lua"

local json = require('json')
function readAll(file)
	local f = io.open(file, "rb")
	local content = f:read("*all")
	f:close()
	return content
end

for file in lfs.dir(plugins_path) do
	if (file~="." and file~=".." and lfs.attributes(plugins_path .. "/" .. file,"mode")=="directory") then
		local filename = plugins_path .. "/" .. file .. "/plugin.json"
		local meta = json.parse(readAll(filename))
		if (meta["plugin"]["type"]=="plugin") and (mame_manager:plugins().entries[meta["plugin"]["name"]]~=nil) then
			local entry = mame_manager:plugins().entries[meta["plugin"]["name"]]	
			if (entry:value()==true) then
				emu.print_verbose("Starting plugin " .. meta["plugin"]["name"] .. "...")
				plugin = require(meta["plugin"]["name"])
				if plugin.set_folder~=nil then plugin.set_folder(plugins_path .. "/" .. file) end
				plugin.startplugin();
			end
		end
	end
end

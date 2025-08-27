add_rules("mode.debug", "mode.release")
set_encodings("utf-8")
set_policy("build.warning", true)
set_languages("cxxlatest")
set_optimize("fastest")
add_requires("microsoft-detours")
add_requires("ftxui")
add_requires("soundtouch", { configs = { shared = true } })

target("audiospeedhack")
set_kind("shared")
add_files("src/audiospeedhack.cpp")
add_packages("microsoft-detours")
add_packages("soundtouch")
add_syslinks("dxguid", "ole32", "oleaut32", "mmdevapi", "user32", "uuid")
add_defines("NOMINMAX")
after_build(function(target)
	-- 1. 获取目标的所有依赖包
	local pkgs = target:pkgs()
	if pkgs then
		-- 2. 遍历所有包，找到我们需要的 'soundtouch'
		for _, pkg in pairs(pkgs) do
			if pkg:name() == "soundtouch" then
				-- 3. 获取包的安装路径下的 bin 目录 (通常存放 dll)
				local dll_dir = path.join(pkg:installdir("bin"), "bin")
				print("SoundTouch DLL dir: %s", dll_dir)
				-- 4. 检查目录是否存在且其中有文件
				if os.isdir(dll_dir) and #os.files(path.join(dll_dir, "*.dll")) > 0 then
					-- 5. 将该目录下的所有 dll 文件拷贝到当前目标的输出目录
					print("Copying DLL to %s", target:targetdir())
					os.cp(path.join(dll_dir, "*.dll"), target:targetdir())
				end
			end
		end
	end
end)

target("injector")
set_kind("binary")
add_files("src/injector.cpp")
add_packages("ftxui")
add_defines("NOMINMAX")

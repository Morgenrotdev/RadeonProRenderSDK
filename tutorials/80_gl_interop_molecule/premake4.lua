project "80_gl_interop_molecule"
    kind "ConsoleApp"
    location "../build"
    files { "../80_gl_interop_molecule/**.h", "../80_gl_interop_molecule/**.cpp"} 
    files { "../common/common.cpp","../common/common.h"}

    -- remove filters for Visual Studio
    vpaths { [""] = { "../80_gl_interop_molecule/**.h", "../80_gl_interop_molecule/**.cpp","../common/common.cpp","../common/common.h"} }


    includedirs{ "../../RadeonProRender/inc" } 

	if os.istarget("windows") then
    	includedirs{ "../../3rdParty/glew/include", "../../3rdParty/freeglut/include"} 	
	end
    if os.istarget("macosx") then
    	includedirs{ "../../3rdParty/glew/include"} 	
	end
	
    buildoptions "-std=c++14"

	configuration {"x64"}

	links {"RadeonProRender64", "GLEW"}

	if os.istarget("windows") then
		links {"freeglut"}
    end
    if os.istarget("linux") then
        links {"glut"}
        links {"GL"}
	    links {"pthread"}
    end
    if os.istarget("macosx") then
         linkoptions{"-framework OpenGL", "-framework GLUT"}   
    end
    
    
	if os.istarget("windows") then
		libdirs {"../../3rdParty/glew/libWin64", "../../3rdParty/freeglut/lib/x64"}
	end
	if os.istarget("macosx") then
		libdirs {"../../3rdParty/glew/libOsx64"}
	end
    
    configuration {"x64", "Debug"}
        targetdir "../Bin"
    configuration {"x64", "Release"}
        targetdir "../Bin"
    configuration {}
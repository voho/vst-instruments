# The JUCE Framework

The JUCE Framework is an open source framework licensed under a combination of
open source and commercial licences.

The JUCE Framework modules are dual-licensed under the
[AGPLv3](https://www.gnu.org/licenses/agpl-3.0.en.html) and the commercial [JUCE
licence](https://juce.com/legal/juce-8-licence/).

## The JUCE Licence

If you are not licensing the JUCE Framework modules under the
[AGPLv3](https://www.gnu.org/licenses/agpl-3.0.en.html) then by downloading,
installing, or using the JUCE Framework, or combining the JUCE Framework with
any other source code, object code, content or any other copyrightable work, you
agree to the terms of the [JUCE 8 End User Licence
Agreement](https://juce.com/legal/juce-8-licence/), and all incorporated terms
including the [JUCE Privacy Policy](https://juce.com/legal/juce-privacy-policy/)
and the [JUCE Website Terms of
Service](https://juce.com/legal/juce-website-terms-of-service/), as applicable,
which will bind you. If you do not agree to the terms of this Agreement, we will
not license the JUCE Framework to you, and you must discontinue the installation
or download process and cease use of the JUCE Framework.

THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF MERCHANTABILITY OR FITNESS
FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

For more information, visit the [JUCE website](https://juce.com).

Full licence terms:
- [JUCE 8 End User Licence Agreement](https://juce.com/legal/juce-8-licence/)
- [JUCE Privacy Policy](https://juce.com/legal/juce-privacy-policy/)
- [JUCE Website Terms of Service](https://juce.com/legal/juce-website-terms-of-service/)

## The JUCE Framework Dependencies

The JUCE modules contain the following dependencies:
- [AudioUnitSDK](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_plugin_client/AU/AudioUnitSDK/) ([Apache 2.0](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt))
- [Oboe](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_devices/native/oboe/) ([Apache 2.0](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_devices/native/oboe/LICENSE))
- [FLAC](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_formats/codecs/flac/) ([BSD](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_formats/codecs/flac/Flac%20Licence.txt))
- [GLEW](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_opengl/opengl/juce_gl.h) ([BSD](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_opengl/opengl/juce_gl.h)), including [Mesa](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_opengl/opengl/juce_gl.h) ([MIT](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_opengl/opengl/juce_gl.h)) and [Khronos](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_opengl/opengl/juce_gl.h) ([MIT](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_opengl/opengl/juce_gl.h))
- [Ogg Vorbis](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_formats/codecs/oggvorbis/) ([BSD](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_formats/codecs/oggvorbis/Ogg%20Vorbis%20Licence.txt))
- [jpeglib](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/image_formats/jpglib/) ([Independent JPEG Group License](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/image_formats/jpglib/README))
- [CHOC](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_javascript/choc/) ([ISC](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_javascript/choc/LICENSE.md)), including [QuickJS](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_javascript/choc/javascript/choc_javascript_QuickJS.h) ([MIT](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_javascript/choc/javascript/choc_javascript_QuickJS.h))
- [LV2](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_processors_headless/format_types/LV2_SDK/) ([ISC](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_processors_headless/format_types/LV2_SDK/lv2/COPYING))
- [pslextensions](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_processors_headless/format_types/pslextensions/ipslcontextinfo.h) ([Public domain](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_processors_headless/format_types/pslextensions/ipslcontextinfo.h))
- [AAX](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_plugin_client/AAX/SDK/) ([Proprietary Avid AAX License/GPLv3](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_plugin_client/AAX/SDK/LICENSE.txt))
- [VST3](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_processors_headless/format_types/VST3_SDK/) ([MIT](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt))
- [Box2D](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_box2d/box2d/) ([zlib](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_box2d/box2d/Box2D.h))
- [pnglib](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/image_formats/pnglib/) ([zlib](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/image_formats/pnglib/LICENSE))
- [zlib](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_core/zip/zlib/) ([zlib](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_core/zip/zlib/README))
- [HarfBuzz](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/fonts/harfbuzz/) ([Old MIT](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/fonts/harfbuzz/COPYING))
- [SheenBidi](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/unicode/sheenbidi/) ([Apache](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_graphics/unicode/sheenbidi/LICENSE))
- [ASIO](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_devices/native/asio/) ([Proprietary Steinberg ASIO License/GPLv3](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/modules/juce_audio_devices/native/asio/LICENSE.txt))

The JUCE examples are licensed under the terms of the
[ISC license](http://www.isc.org/downloads/software-support-policy/isc-license/).

Dependencies in the examples:
- [reaper-sdk](https://github.com/juce-framework/JUCE/tree/2cdfca8feb300fb424002ba2c2751569e5bacb64/examples/Plugins/extern/) ([zlib](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/examples/Plugins/extern/LICENSE.md))

Dependencies in the bundled applications:
- [Projucer icons](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/extras/Projucer/Source/Utility/UI/jucer_Icons.cpp) ([MIT](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/extras/Projucer/Source/Utility/UI/jucer_Icons.cpp))

Dependencies in the build system:
- [Android Gradle](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/examples/DemoRunner/Builds/Android/gradle/wrapper/LICENSE-for-gradlewrapper.txt) ([Apache 2.0](https://github.com/juce-framework/JUCE/blob/2cdfca8feb300fb424002ba2c2751569e5bacb64/examples/DemoRunner/Builds/Android/gradle/wrapper/LICENSE-for-gradlewrapper.txt))

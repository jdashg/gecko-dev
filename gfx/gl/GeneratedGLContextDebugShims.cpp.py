import FuncXFM

def TransformGL(funcName, argList, retType):
    outputDict = dict()

    assert funcName.startswith('gl'), 'funcName: ' + funcName
    funcName = funcName[2:]

    outputDict['func names'] = funcName

    ################

    template = '''\
    static {RetType}
    gl{FuncName}({ArgDecls})
    {{
        const char kFuncName[] = "gl{FuncName}";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, {SkipErrorCheck});
        {MaybeRetDecl}gl->mOriginalSymbols.f{FuncName}({ArgNames});
        gl->Debug_AfterGLCall(kFuncName, {SkipErrorCheck});{MaybeReturn}
    }}'''

    argDecls = []
    argNames = []
    for (type, name) in argList:
        argDecls.append(type + ' ' + name)
        argNames.append(name)
        continue
    argDecls = ', '.join(argDecls)
    argNames = ', '.join(argNames)

    maybeRetDecl = ''
    maybeReturn = ''
    if retType != 'void':
        maybeRetDecl = 'const auto ret = '
        maybeReturn = '\n\n' + ' '*8 + 'return ret;'

    skipErrorCheck = 'false'
    if funcName in ['GetError', 'Finish']:
        skipErrorCheck = 'true'

    debugShim = template.format(RetType=retType,
                                FuncName=funcName,
                                ArgDecls=argDecls,
                                SkipErrorCheck=skipErrorCheck,
                                MaybeRetDecl=maybeRetDecl,
                                ArgNames=argNames,
                                MaybeReturn=maybeReturn)

    outputDict['debugShims'] = debugShim

    ################

    assignShim = ' '*4 + 'mSymbols.f{0} = GLContextDebugShims::gl{0};'.format(funcName)
    outputDict['assignShims'] = assignShim

    return outputDict


funcList = FuncXFM.CFuncList('gl.funclist')
funcList.Sort(lambda(funcName, argList, retType): funcName.lower())

outputListDict = funcList.Transform(TransformGL)

template = '''\
// GENERATED FILE - `python GeneratedGLContextDebugShims.cpp.py`
#include "GLContext.h"

namespace mozilla {{
namespace gl {{

struct GLContextDebugShims
{{
{DebugShims}
}};

void
GLContext::ShimDebugSymbols()
{{
{AssignShims}
}}

}} // namespace gl
}} // namespace mozilla
'''

debugShims = '\n\n'.join(outputListDict['debugShims'])
assignShims = '\n'.join(outputListDict['assignShims'])

output = template.format(DebugShims=debugShims,
                         AssignShims=assignShims)

with open('GeneratedGLContextDebugShims.cpp', 'wb') as f:
    f.write(output)


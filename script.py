import os, re
files = [
    'Src/Integration/src/TimeIntegrator.cpp',
    'Src/Integration/src/CentralDifference.cpp',
    'Src/Integration/src/ExplicitEuler.cpp',
    'Src/Integration/src/ADR_Integrator.cpp',
    'Src/Integration/src/StaggeredIntegrator.cpp',
    'PDCommon/Contact/src/SillingContact.cpp',
    'PDCommon/Contact/src/KinematicContact.cpp',
    'PDCommon/Contact/src/PenaltyContact.cpp'
]
vars_to_convert = ['dt_', 'dx', 'maxWaveSpeed', 'massScale', 'dt', 'limitDt', 'minDt', 'currentDt', 'targetTime', 'currentTime', 'timer.pureComputeTime()', 'timer.totalElapsed()', 'elapsed', 'totalElapsed', 'massScaleFactor_', 'currentPhysicalTime', 'currentLocalLF', 'c_s', 'K_contact', 'config.userDt', 'restitutionCoeff_', 'stiffnessFactor_', 'horizon', 'K']
for f in files:
    f_path = 'd:/Project_C++/GRPD/' + f
    if not os.path.exists(f_path): continue
    with open(f_path, 'r', encoding='utf-8') as file:
        content = file.read()
    
    if 'StringUtils.h' not in content:
        if '#include "Logger.h"' in content:
            content = content.replace('#include "Logger.h"', '#include "Logger.h"\n#include "StringUtils.h"')
        elif '#include "PDContext.h"' in content:
            content = content.replace('#include "PDContext.h"', '#include "PDContext.h"\n#include "StringUtils.h"')

    for v in vars_to_convert:
        v_escaped = v.replace('(', r'\(').replace(')', r'\)')
        pattern = r'std::to_string\(' + v_escaped + r'\)'
        content = re.sub(pattern, 'PDCommon::Utils::StringUtils::toScientific(' + v + ')', content)
        
    with open(f_path, 'w', encoding='utf-8') as file:
        file.write(content)

#include "RootAccess.h"

// الدوال الفعلية في src/jbc (C). نعلنها هنا بدون تضمين defs.h لتجنب تعارض typedefs مع SDK.
extern "C" {
uintptr_t jbc_get_prison0(void);
int jbc_get_cred(RootCred* cred);
int jbc_jailbreak_cred(RootCred* cred);
int jbc_set_cred(const RootCred* cred);
}

bool RootAccess::Initialize() {
    if (m_active) return true;
    RootCred orig{};
    if (jbc_get_cred(&orig) != 0) return false;   // قراءة فقط
    if (jbc_get_prison0() == 0) return false;     // قراءة فقط
    m_probeOk = true;
    m_orig = orig;
#if ROOT_STAGE < 2
    return false;   // مرحلة الاختبار: لا نغيّر الصلاحيات
#else
    RootCred elevated = orig;
    if (jbc_jailbreak_cred(&elevated) != 0) return false;
    if (jbc_set_cred(&elevated) != 0) return false;
    m_active = true;
    return true;
#endif
}

void RootAccess::Terminate() {
    if (!m_active) return;
    jbc_set_cred(&m_orig);
    m_active = false;
}

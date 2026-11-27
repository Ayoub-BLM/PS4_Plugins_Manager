#pragma once
#include <stdint.h>

// المرحلة 1: قراءة فقط (لا تغيّر أي صلاحية). المرحلة 2: الهروب الفعلي من الـ sandbox.
#ifndef ROOT_STAGE
#define ROOT_STAGE 2
#endif

// تطابق struct jbc_cred في src/jbc/jailbreak.h (نفس الترتيب والأحجام)
struct RootCred {
    uint32_t uid, ruid, svuid;
    uint32_t rgid, svgid;
    uintptr_t prison, cdir, rdir, jdir;
    uint64_t sceProcType, sonyCred, sceProcCap;
};

// غلاف بسيط فوق libjbc: يهرب من الـ sandbox ويرجع منه.
// يتطلب أن يكون الجهاز مجلبراً مسبقاً (GoldHEN + exploit فعّال)، وإلا قد يتعطل التطبيق.
class RootAccess {
public:
    bool Initialize();   // true إذا نجح الهروب من الـ sandbox
    void Terminate();    // يرجّع الصلاحيات الأصلية
    bool IsJailbroken() const { return m_active; }
    bool ProbeOk() const { return m_probeOk; }   // نجحت قراءة الكيرنل
private:
    bool m_active = false;
    bool m_probeOk = false;
    RootCred m_orig{};
};

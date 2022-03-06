
// ndrx_pymod.h
#ifndef NDRX_PYMOD_H
#define NDRX_PYMOD_H

#include <atmi.h>
#include <tpadm.h>
#include <userlog.h>
#include <xa.h>
#include <ubf.h>
#undef _

/**
 * XATMI buffer handling routines
 */
class xatmibuf
{
public:
    xatmibuf();
    xatmibuf(TPSVCINFO *svcinfo);
    xatmibuf(const char *type, long len);
    void reinit(const char *type, long len_);

    xatmibuf(const xatmibuf &) = delete;
    xatmibuf &operator=(const xatmibuf &) = delete;
    
    xatmibuf(xatmibuf &&other);
    xatmibuf &operator=(xatmibuf &&other);
    ~xatmibuf();

    

    char *release();
    UBFH **fbfr();
    char **pp;
    char *p;
    long len;
    void mutate(std::function<int(UBFH *)> f);

private:
    void swap(xatmibuf &other) noexcept;
};

#endif /* NDRX_PYMOD.H */
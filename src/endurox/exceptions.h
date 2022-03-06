#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/**
 * XATMI Sub-systme exception
 */
struct xatmi_exception : public std::exception
{
private:
    int code_;
    std::string message_;

protected:
    xatmi_exception(int code, const std::string &message)
        : code_(code), message_(message) {}

public:
    explicit xatmi_exception(int code)
        : code_(code), message_(tpstrerror(code)) {}

    const char *what() const noexcept override { return message_.c_str(); }
    int code() const noexcept { return code_; }
};

/**
 * TMQ persisten queue related exceptions.
 * returned from tpenqueue(), tpdequeue().
 */
struct qm_exception : public xatmi_exception
{
public:
    explicit qm_exception(int code) : xatmi_exception(code, qmstrerror(code)) {}

    static const char *qmstrerror(int code)
    {
        switch (code)
        {
        case QMEINVAL:
            return "An invalid flag value was specified.";
        case QMEBADRMID:
            return "An invalid resource manager identifier was specified.";
        case QMENOTOPEN:
            return "The resource manager is not currently open.";
        case QMETRAN:
            return "Transaction error.";
        case QMEBADMSGID:
            return "An invalid message identifier was specified.";
        case QMESYSTEM:
            return "A system error occurred. The exact nature of the error is "
                   "written to a log file.";
        case QMEOS:
            return "An operating system error occurred.";
        case QMEABORTED:
            return "The operation was aborted.";
        case QMEPROTO:
            return "An enqueue was done when the transaction state was not active.";
        case QMEBADQUEUE:
            return "An invalid or deleted queue name was specified.";
        case QMENOSPACE:
            return "Insufficient resources.";
        case QMERELEASE:
            return "Unsupported feature.";
        case QMESHARE:
            return "Queue is opened exclusively by another application.";
        case QMENOMSG:
            return "No message was available for dequeuing.";
        case QMEINUSE:
            return "Message is in use by another transaction.";
        default:
            return "?";
        }
    }
};

/**
 * UBF buffer related exception handling
 */
struct ubf_exception : public std::exception
{
private:
    int code_;
    std::string message_;

public:
    explicit ubf_exception(int code)
        : code_(code), message_(Bstrerror(code)) {}

    const char *what() const noexcept override { return message_.c_str(); }
    int code() const noexcept { return code_; }
};

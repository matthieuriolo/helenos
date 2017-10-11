/*
 * Copyright (c) 2017 Jaroslav Jindrak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

namespace __cxxabiv1
{
    /**
     * Stack unwinding functionality - Level 1.
     */

    enum _Unwind_Reason_Code
    {
        _URC_NO_REASON                = 0,
        _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
        _URC_FATAL_PHASE2_ERROR       = 2,
        _URC_FATAL_PHASE1_ERROR       = 3,
        _URC_NORMAL_STOP              = 4,
        _URC_END_OF_STACK             = 5,
        _URC_HANDLER_FOUND            = 6,
        _URC_INSTALL_CONTEXT          = 7,
        _URC_CONTINUE_UNWIND          = 8
    };

    struct _Unwind_Exception;
    using _Unwind_Exception_Cleanup_Fn = void (*)(_Unwind_Reason_Code, _Unwind_Exception*);

    struct _Unwind_Exception
    {
        std::uint64_t exception_class;
        _Unwind_Exception_Cleanup_Fn exception_cleanup;
        std::uint64_t private_1;
        std::uint64_t private_2;
    };

    /* Opaque structure. */
    struct _Unwind_Context;

    /**
     * TODO: Explain parameter semantics.
     */
    using _Unwind_Stop_Fn = _Unwind_Reason_Code(*)(
        int, _Unwind_Action, std::uint64_t, _Unwind_Exception*,
        _Unwind_Context*, void*
    );

    _Unwind_Reason_Code _Unwind_ForcedUnwind(_Unwind_Exception*, _Unwind_Stop_Fn, void*)
    {
        // TODO: implement
    }

    void _Unwind_Resume(_Unwind_Exception*)
    {
        // TODO: implement
    }

    void _Unwind_DeleteException(_Unwind_Exception*)
    {
        // TODO: implement
    }

    std::uint64_t _Unwind_GetGR(_Unwind_Context*, int)
    {
        // TODO: implement
    }

    void _Unwind_SetGR(_Unwind_Context*, int, std::uint64_t)
    {
        // TODO: implement
    }

    std::uint64_t _Unwind_GetIP(_Unwind_Context*)
    {
        // TODO: implement
    }

    void _Unwind_SetIP(_Unwind_Context*, std::uint64_t)
    {
        // TODO: implement
    }

    std::uint64_t _Unwind_GetLanguageSpecificData(_Unwind_Context*)
    {
        // TODO: implement
    }

    std::uint64_t _Unwind_GetRegionStart(_Unwind_Context*)
    {
        // TODO: implement
    }

    /**
     * TODO: Explain parameter semantics.
     */
    using __personality_routine = _Unwind_Reason_Code(*)(
        int, _Unwind_Action, std::uint64_t, _Unwind_Exception*,
        _Unwind_Context*, void*
    );

    /**
     * Stack unwinding functionality - Level 2.
     */
    // TODO:
}
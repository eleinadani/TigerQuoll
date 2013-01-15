/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the PJs project (https://github.com/mozilla/pjs/).
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nicholas Matsakis <nmatsakis@mozilla.com>
 *   Donovan Preston <dpreston@mozilla.com>
 *   Fadi Meawad <fmeawad@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef PJSUTIL_H_
#define PJSUTIL_H_


namespace pjs {

class AutoContextPrivate {
private:
	JSContext *_cx;

public:
	AutoContextPrivate(JSContext *cx, void *v) :
			_cx(cx) {
		JS_SetContextPrivate(_cx, v);
	}

	~AutoContextPrivate() {
		JS_SetContextPrivate(_cx, NULL);
	}
};

class AutoLock
{
  private:
    PRLock *lock;

  public:
    AutoLock(PRLock *lock) : lock(lock) { PR_Lock(lock); }
    ~AutoLock() { PR_Unlock(lock); }
};

template<class T>
class auto_arr
{
private:
    T* ap;    // refers to the actual owned object (if any)

public:
    typedef T element_type;

    // constructor
    explicit auto_arr (T* ptr = 0) throw() : ap(ptr) { }

    // copy constructors (with implicit conversion)
    // - note: nonconstant parameter
    auto_arr (auto_arr& rhs) throw() : ap(rhs.release()) { }

    template<class Y>
    auto_arr (auto_arr<Y>& rhs) throw() : ap(rhs.release()) { }

    // assignments (with implicit conversion)
    // - note: nonconstant parameter
    auto_arr& operator= (auto_arr& rhs) throw()
    {
        reset(rhs.release());
        return *this;
    }
    template<class Y>
    auto_arr& operator= (auto_arr<Y>& rhs) throw()
    {
        reset(rhs.release());
        return *this;
    }

    // destructor
    ~auto_arr() throw()
    {
        delete[] ap;
    }

    // value access
    T* get() const throw()
    {
        return ap;
    }

    T& operator*() const throw()
    {
        return *ap;
    }

    T& operator[](int i) const throw()
    {
        return ap[i];
    }

    T* operator->() const throw()
    {
        return ap;
    }

    // release ownership
    T* release() throw()
    {
        T* tmp(ap);
        ap = 0;
        return tmp;
    }

    // reset value
    void reset (T* ptr=0) throw()
    {
        if (ap != ptr)
        {
            delete[] ap;
            ap = ptr;
        }
    }
};

}




#endif /* PJSUTIL_H_ */

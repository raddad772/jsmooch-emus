//
// Created by . on 8/11/24.
//

#ifndef JSMOOCH_EMUS_OOC_H
#define JSMOOCH_EMUS_OOC_H

#define DTOR_child(getfrom, kname)  kname##_delete(&this-> getfrom);
#define DTOR_child_cvec(getfrom, kname) for (u32 i = 0; i < cvec_len(&this-> getfrom); i++) {\
                                    kname##_delete(cvec_get(&this-> getfrom, i));\
                                    }\
                                    cvec_delete(&this-> getfrom);

#define CTOR_ZERO_SELF memset(this, 0, sizeof(*this))


#endif //JSMOOCH_EMUS_OOC_H

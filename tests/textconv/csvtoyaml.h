//
//  csvtoyaml.h
//  libgit2_clar
//
//  Created by Local Administrator on 12/28/17.
//

#ifndef csvtoyaml_h
#define csvtoyaml_h

#include "git2/sys/textconv.h"

extern git_textconv *create_csv_to_yaml_textconv(void);

#endif /* csvtoyaml_h */

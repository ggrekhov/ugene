/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
 * http://ugene.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef _REMOTE_QUERY_WORKER_
#define _REMOTE_QUERY_WORKER_

#include <U2Lang/LocalDomain.h>
#include <U2Lang/WorkflowUtils.h>

#include "RemoteBLASTTask.h"

namespace U2 {
namespace LocalWorkflow {

class RemoteBLASTPrompter;
typedef PrompterBase<RemoteBLASTPrompter> RemoteBLASTPrompterBase;

class RemoteBLASTPrompter : public RemoteBLASTPrompterBase {
    Q_OBJECT
public:
    RemoteBLASTPrompter(Actor* p = 0) : RemoteBLASTPrompterBase(p) {}
protected:
    QString composeRichDoc();
};


class RemoteBLASTWorker: public BaseWorker {
    Q_OBJECT
public:
    RemoteBLASTWorker(Actor *a) : BaseWorker(a), input(NULL), output(NULL) {}
    virtual void init();
    virtual Task* tick();
    virtual void cleanup() {};
private slots:
    void sl_taskFinished();
protected:
    IntegralBus *input, *output;
    //QString resultName,transId;
    RemoteBLASTTaskSettings cfg;
};

class RemoteBLASTWorkerFactory:public DomainFactory {
    static const QString ACTOR_ID;
public:
    static void init();
    RemoteBLASTWorkerFactory() : DomainFactory(ACTOR_ID) {}
    virtual Worker* createWorker(Actor* a) {return new RemoteBLASTWorker(a);}
};

}
}




#endif

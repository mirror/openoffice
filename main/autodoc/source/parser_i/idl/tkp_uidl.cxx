/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/

#include <precomp.h>
#include <s2_luidl/tkp_uidl.hxx>

// NOT FULLY DECLARED SERVICES
#include <s2_luidl/cx_idlco.hxx>



namespace csi
{
namespace uidl
{


TokenParser_Uidl::TokenParser_Uidl( Token_Receiver &		o_rUidlReceiver,
									DYN ::TkpDocuContext &	let_drDocuContext )
	:	pBaseContext(new Context_UidlCode(o_rUidlReceiver, let_drDocuContext)),
        pCurContext(0)
{
	SetStartContext();
}

TokenParser_Uidl::~TokenParser_Uidl()
{
}

void
TokenParser_Uidl::SetStartContext()
{
	pCurContext = pBaseContext.Ptr();
}

void
TokenParser_Uidl::SetCurrentContext( TkpContext & io_rContext )
{
	pCurContext = &io_rContext;
}

TkpContext &
TokenParser_Uidl::CurrentContext()
{
	return *pCurContext;
}

}   // namespace uidl
}   // namespace csi


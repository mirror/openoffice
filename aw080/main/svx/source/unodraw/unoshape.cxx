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



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_svx.hxx"

#define _SVX_USE_UNOGLOBALS_
#include <cppuhelper/typeprovider.hxx>
#include <com/sun/star/awt/XBitmap.hpp>
#include <com/sun/star/awt/Rectangle.hpp>
#include <com/sun/star/drawing/CircleKind.hpp>
#include <com/sun/star/embed/NoVisualAreaSizeException.hpp>
#include <vcl/svapp.hxx>
#include <svl/itemprop.hxx>
#include <svtools/fltcall.hxx>
#include <vos/mutex.hxx>
#include <editeng/unotext.hxx>
#include <svx/svdobj.hxx>
#include <svx/svdoole2.hxx>
#include <osl/mutex.hxx>
#include <comphelper/extract.hxx>
#include "svx/shapepropertynotifier.hxx"
#include <comphelper/serviceinfohelper.hxx>
#include <toolkit/unohlp.hxx>
#include <comphelper/serviceinfohelper.hxx>
#include <rtl/uuid.h>
#include <rtl/memory.h>
#include <vcl/gfxlink.hxx>
#include <vcl/virdev.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/viewsh.hxx>
#include "svx/svdopage.hxx"
#include "svx/xflbstit.hxx"
#include "svx/xflbmtit.hxx"
#include "svx/xlnstit.hxx"
#include "svx/xlnedit.hxx"
#include "svx/svdogrp.hxx"
#include "svx/scene3d.hxx"
#include "svx/svdmodel.hxx"
#include "svx/globl3d.hxx"
#include "svx/fmglob.hxx"
#include "svx/unopage.hxx"
#include "svx/view3d.hxx"
#include "svx/unoshape.hxx"
#include "svx/svxids.hrc"
#include "svx/unoshtxt.hxx"
#include "svx/svdpage.hxx"
#include "svx/unoshprp.hxx"
#include "svx/svdograf.hxx"
#include "svx/unoapi.hxx"
#include "svx/svdomeas.hxx"
#include "svx/svdpagv.hxx"
#include "svx/svdpool.hxx"
#include <tools/shl.hxx>	//
#include "svx/dialmgr.hxx"		// not nice, we need our own resources some day
#include "svx/dialogs.hrc"		//
#include "svx/svdocapt.hxx"
#include <svx/obj3d.hxx>
#include <tools/diagnose_ex.h>
#include "svx/xflftrit.hxx"
#include "svx/xtable.hxx"
#include "svx/xbtmpit.hxx"
#include "svx/xflgrit.hxx"
#include "svx/xflhtit.hxx"
#include "svx/xlnedit.hxx"
#include "svx/xlnstit.hxx"
#include "svx/xlndsit.hxx"
#include "svx/svdglob.hxx"
#include "svx/svdstr.hrc"
#include "svx/unomaster.hxx"
#include <editeng/outlobj.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <vector>
#include "svx/lathe3d.hxx"
#include "svx/extrud3d.hxx"
#include "unopolyhelper.hxx"
#include <comphelper/scopeguard.hxx>
#include <boost/bind.hpp>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <svx/svdlegacy.hxx>
#include <svx/svdocirc.hxx>
#include <svx/globaldrawitempool.hxx>

using ::rtl::OUString;
using namespace ::osl;
using namespace ::vos;
using namespace ::cppu;
using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::container;
using ::svx::PropertyValueProvider;
using ::svx::IPropertyValueProvider;

#define QUERYINT( xint ) \
	if( rType == ::getCppuType((const uno::Reference< xint >*)0) ) \
		aAny <<= uno::Reference< xint >(this)

class GDIMetaFile;
class SvStream;
sal_Bool ConvertGDIMetaFileToWMF( const GDIMetaFile & rMTF, SvStream & rTargetStream,
							  FilterConfigItem* pFilterConfigItem = NULL, sal_Bool bPlaceable = sal_True );

uno::Reference< uno::XInterface > SAL_CALL SvxUnoGluePointAccess_createInstance( SdrObject* pObject );

/***********************************************************************
* class SvxShapeImpl                                                   *
***********************************************************************/

struct SvxShapeImpl
{
    SvxShape&       mrAntiImpl;
	SfxItemSet*		mpItemSet;
	sal_uInt32		mnObjId;
	SvxShapeMaster*	mpMaster;
	bool			mbHasSdrObjectOwnership;
	bool            mbDisposing;

    /** CL, OD 2005-07-19 #i52126# - this is initially 0 and set when
     *  a SvxShape::Create() call is executed. It is then set to the created
     *  SdrObject so a multiple call to SvxShape::Create() with same SdrObject
     *  is prohibited.
     */
    ::tools::WeakReference< SdrObject > mpCreatedObj;

    // for xComponent
	::cppu::OInterfaceContainerHelper   maDisposeListeners;
    ::svx::PropertyChangeNotifier       maPropertyNotifier;

    SvxShapeImpl( SvxShape& _rAntiImpl, ::osl::Mutex& _rMutex )
        :mrAntiImpl( _rAntiImpl )
        ,mpItemSet( NULL )
	    ,mnObjId( 0 )
	    ,mpMaster( NULL )
	    ,mbHasSdrObjectOwnership( false )
        ,mbDisposing( false )
        ,mpCreatedObj()
        ,maDisposeListeners( _rMutex )
        ,maPropertyNotifier( _rAntiImpl, _rMutex )
    {
    }
};

/**********************************************************************/
class ShapePositionProvider : public PropertyValueProvider
{
public:
    ShapePositionProvider( const SvxShapeImpl& _shapeImpl )
        :PropertyValueProvider( _shapeImpl.mrAntiImpl, "Position" )
    {
    }

protected:
    virtual void getCurrentValue( Any& _out_rCurrentValue ) const
    {
        _out_rCurrentValue <<= static_cast< SvxShape& >( getContext() ).getPosition();
    }
};

//----------------------------------------------------------------------
class ShapeSizeProvider : public PropertyValueProvider
{
public:
    ShapeSizeProvider( const SvxShapeImpl& _shapeImpl )
        :PropertyValueProvider( _shapeImpl.mrAntiImpl, "Size" )
    {
    }

protected:
    virtual void getCurrentValue( Any& _out_rCurrentValue ) const
    {
        _out_rCurrentValue <<= static_cast< SvxShape& >( getContext() ).getSize();
    }
};

/***********************************************************************
* class SvxShape                                                       *
***********************************************************************/

SvxShape::SvxShape( SdrObject* pObject ) throw()
:	maSize(100,100)
,	mpImpl( new SvxShapeImpl( *this, maMutex ) )
,	mbIsMultiPropertyCall(false)
,	mpPropSet(aSvxMapProvider.GetPropertySet(SVXMAP_SHAPE, GetGlobalDrawObjectItemPool()))
,   maPropMapEntries(aSvxMapProvider.GetMap(SVXMAP_SHAPE))
,	mpObj(pObject)
,	mpModel(NULL)
,	mnLockCount(0)
{
	impl_construct();
}

//----------------------------------------------------------------------
SvxShape::SvxShape( SdrObject* pObject, const SfxItemPropertyMapEntry* pEntries, const SvxItemPropertySet* pPropertySet ) throw()
:	maSize(100,100)
,	mpImpl( new SvxShapeImpl( *this, maMutex ) )
,	mbIsMultiPropertyCall(false)
,	mpPropSet(pPropertySet)
,   maPropMapEntries(pEntries)
,	mpObj(pObject)
,	mpModel(NULL)
,	mnLockCount(0)
{
	impl_construct();
}

//----------------------------------------------------------------------
SvxShape::SvxShape() throw()
:	maSize(100,100)
,	mpImpl( new SvxShapeImpl( *this, maMutex ) )
,	mbIsMultiPropertyCall(false)
,	mpPropSet(aSvxMapProvider.GetPropertySet(SVXMAP_SHAPE, GetGlobalDrawObjectItemPool()))
,   maPropMapEntries(aSvxMapProvider.GetMap(SVXMAP_SHAPE))
,	mpObj(NULL)
,	mpModel(NULL)
,	mnLockCount(0)
{
	impl_construct();
}

//----------------------------------------------------------------------
SvxShape::~SvxShape() throw()
{
	OGuard aGuard( Application::GetSolarMutex() );

	DBG_ASSERT( mnLockCount == 0, "Locked shape was disposed!" );

	if ( mpModel )
		EndListening( *mpModel );

	if ( mpImpl->mpMaster )
		mpImpl->mpMaster->dispose();

    if ( mpObj.is() )
    {
        SetUnoShapeAtSdrObjectFromSvxShape(*mpObj.get(), 0);
    }

	if( HasSdrObjectOwnership() && mpObj.is() )
	{
		mpImpl->mbHasSdrObjectOwnership = false;
		SdrObject* pObject = mpObj.get();
		deleteSdrObjectSafeAndClearPointer( pObject );
	}

	delete mpImpl, mpImpl = NULL;
}

//----------------------------------------------------------------------

void SvxShape::TakeSdrObjectOwnership()
{
	mpImpl->mbHasSdrObjectOwnership = true;
}

//----------------------------------------------------------------------

bool SvxShape::HasSdrObjectOwnership() const
{
    if ( !mpImpl->mbHasSdrObjectOwnership )
        return false;

    OSL_ENSURE( mpObj.is(), "SvxShape::HasSdrObjectOwnership: have the ownership of an object which I don't know!" );
    return mpObj.is();
}

//----------------------------------------------------------------------

void SvxShape::setShapeKind( sal_uInt32 nKind )
{
	mpImpl->mnObjId = nKind;
}

//----------------------------------------------------------------------

sal_uInt32 SvxShape::getShapeKind() const
{
	return mpImpl->mnObjId;
}

//----------------------------------------------------------------------

void SvxShape::setMaster( SvxShapeMaster* pMaster )
{
	mpImpl->mpMaster = pMaster;
}

SvxShapeMaster* SvxShape::getMaster()
{
	return mpImpl->mpMaster;
}

const SvxShapeMaster* SvxShape::getMaster() const
{
	return mpImpl->mpMaster;
}

//----------------------------------------------------------------------

uno::Any SAL_CALL SvxShape::queryAggregation( const uno::Type& rType ) throw (uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		uno::Any aAny;
		if( mpImpl->mpMaster->queryAggregation( rType, aAny ) )
			return aAny;
	}

    return SvxShape_UnoImplHelper::queryAggregation(rType);
}

//----------------------------------------------------------------------
const ::com::sun::star::uno::Sequence< sal_Int8 > & SvxShape::getUnoTunnelId() throw()
{
	static ::com::sun::star::uno::Sequence< sal_Int8 > * pSeq = 0;
	if( !pSeq )
	{
		::osl::Guard< ::osl::Mutex > aGuard( ::osl::Mutex::getGlobalMutex() );
		if( !pSeq )
		{
			static ::com::sun::star::uno::Sequence< sal_Int8 > aSeq( 16 );
			rtl_createUuid( (sal_uInt8*)aSeq.getArray(), 0, sal_True );
			pSeq = &aSeq;
		}
	}
	return *pSeq;
}

//----------------------------------------------------------------------
SvxShape* SvxShape::getImplementation( const uno::Reference< uno::XInterface >& xInt )
{
	uno::Reference< lang::XUnoTunnel > xUT( xInt, ::com::sun::star::uno::UNO_QUERY );
	if( xUT.is() )
		return reinterpret_cast<SvxShape*>(sal::static_int_cast<sal_uIntPtr>(xUT->getSomething( SvxShape::getUnoTunnelId())));
	else
		return NULL;
}

//----------------------------------------------------------------------
sal_Int64 SAL_CALL SvxShape::getSomething( const ::com::sun::star::uno::Sequence< sal_Int8 >& rId ) throw(::com::sun::star::uno::RuntimeException) \
{
	if( rId.getLength() == 16 && 0 == rtl_compareMemory( getUnoTunnelId().getConstArray(), rId.getConstArray(), 16 ) )
	{
		return sal::static_int_cast<sal_Int64>(reinterpret_cast<sal_uIntPtr>(this));
	}
	else
	{
		return 0;
	}
}

//----------------------------------------------------------------------
SvxShape* SvxShape::GetShapeForSdrObj( SdrObject* pObj ) throw()
{
	return getImplementation( pObj->getUnoShape() );
}

//----------------------------------------------------------------------
::svx::PropertyChangeNotifier& SvxShape::getShapePropertyChangeNotifier()
{
    return mpImpl->maPropertyNotifier;
}

//----------------------------------------------------------------------
void SvxShape::impl_construct()
{
    mpImpl->maPropertyNotifier.registerProvider( ::svx::eShapePosition,
        ::svx::PPropertyValueProvider( new ShapePositionProvider( *mpImpl ) ) );
    mpImpl->maPropertyNotifier.registerProvider( ::svx::eShapeSize,
        ::svx::PPropertyValueProvider( new ShapeSizeProvider( *mpImpl ) ) );

    if ( mpObj.is() )
        impl_initFromSdrObject();
}

//----------------------------------------------------------------------
void SvxShape::impl_initFromSdrObject()
{
    DBG_TESTSOLARMUTEX();
    OSL_PRECOND( mpObj.is(), "SvxShape::impl_initFromSdrObject: not to be called without SdrObject!" );
    if ( !mpObj.is() )
        return;

    osl_incrementInterlockedCount( &m_refCount );
	{
        SetUnoShapeAtSdrObjectFromSvxShape(*mpObj.get(), *this);
	}
	osl_decrementInterlockedCount( &m_refCount );

	mpModel = &mpObj->getSdrModelFromSdrObject();

	// #i40944#
	// Do not simply return when no model but do the type corrections
	// following below.
	if(mpModel)
	{
		StartListening( *mpModel );
	}

	const sal_uInt32 nInventor = mpObj->GetObjInventor();

	// is it one of ours (svx) ?
	if( nInventor == SdrInventor || nInventor == E3dInventor || nInventor == FmFormInventor )
	{
		if(nInventor == FmFormInventor)
		{
			mpImpl->mnObjId = OBJ_UNO;
		}
		else
		{
			mpImpl->mnObjId = mpObj->GetObjIdentifier();
			if( nInventor == E3dInventor )
				mpImpl->mnObjId |= E3D_INVENTOR_FLAG;
		}

		switch(mpImpl->mnObjId)
		{
		case OBJ_CCUT:			// Kreisabschnitt
		case OBJ_CARC:			// Kreisbogen
		case OBJ_SECT:			// Kreissektor
			mpImpl->mnObjId = OBJ_CIRC;
			break;
		}
	}
}

//----------------------------------------------------------------------
void SvxShape::Create( SdrObject* pNewObj, SvxDrawPage* /*pNewPage*/ )
{
    DBG_TESTSOLARMUTEX();

    OSL_PRECOND( pNewObj, "SvxShape::Create: invalid new object!" );
    if ( !pNewObj )
        return;

    SdrObject* pCreatedObj = mpImpl->mpCreatedObj.get();
    OSL_ENSURE( ( pCreatedObj == NULL ) || ( pCreatedObj == pNewObj ),
        "SvxShape::Create: the same shape used for two different objects?! Strange ..." );

    // --> CL, OD 2005-07-19 #i52126# - correct condition
    if ( pCreatedObj != pNewObj )
    // <--
	{
        // --> CL, OD 2005-07-19 #i52126#
        mpImpl->mpCreatedObj = pNewObj;
        // <--

		if( mpObj.is() )
		{
			EndListening( mpObj->getSdrModelFromSdrObject() );
		}

		mpObj.reset( pNewObj );

        OSL_ENSURE( !mbIsMultiPropertyCall, "SvxShape::Create: hmm?" );
            // this was previously set in impl_initFromSdrObject, but I think it was superfluous
            // (it definitely was in the other context where it was called, but I strongly suppose
            // it was also superfluous when called from here)
		impl_initFromSdrObject();

		ObtainSettingsFromPropertySet( *mpPropSet );

        // here the UserCall was rescured, resetted and restored after the calls
        // to setPosition/setSize. This was done to suppress UserCall handling,
        // thus this can be emulated by removing them temporarily
        ::std::vector< SfxListener* > aOriginalListeners;

        if(mpObj->HasListeners())
        {
            const sal_uInt16 nCount(mpObj->GetListenerCount());

            for(sal_uInt16 a(0); a < nCount; a++)
            {
                SfxListener* pCandidate = mpObj->GetListener(a);

                if(pCandidate)
                {
                    aOriginalListeners.push_back(pCandidate);
                }
            }

            for(sal_uInt32 b(0); b < aOriginalListeners.size(); b++)
            {
				aOriginalListeners[b]->EndListening(*mpObj.get());
            }
        }

		setPosition( maPosition );
		setSize( maSize );

        if(aOriginalListeners.size())
        {
            for(sal_uInt32 b(0); b < aOriginalListeners.size(); b++)
            {
				aOriginalListeners[b]->StartListening(*mpObj.get());
            }
        }

		// if this shape was already named, use this name
		if( maShapeName.getLength() )
		{
			mpObj->SetName( maShapeName );
			maShapeName = OUString();
		}
	}
}

//----------------------------------------------------------------------

void SvxShape::ForceMetricToItemPoolMetric(basegfx::B2DPoint& rPoint) const throw()
{
    DBG_TESTSOLARMUTEX();
	if(mpModel)
	{
		SfxMapUnit eMapUnit = mpModel->GetItemPool().GetMetric(0);
		if(eMapUnit != SFX_MAPUNIT_100TH_MM)
		{
			switch(eMapUnit)
			{
				case SFX_MAPUNIT_TWIP :
				{
					rPoint.setX( MM_TO_TWIPS( rPoint.getX() ) );
					rPoint.setY( MM_TO_TWIPS( rPoint.getY() ) );
					break;
				}
				default:
				{
					DBG_ERROR("AW: Missing unit translation to PoolMetric!");
				}
			}
		}
	}
}

//----------------------------------------------------------------------
void SvxShape::ForceMetricTo100th_mm(basegfx::B2DPoint& rPoint) const throw()
{
    DBG_TESTSOLARMUTEX();
	SfxMapUnit eMapUnit = SFX_MAPUNIT_100TH_MM;
    if(mpModel)
    {
		eMapUnit = mpModel->GetItemPool().GetMetric(0);
        if(eMapUnit != SFX_MAPUNIT_100TH_MM)
        {
            switch(eMapUnit)
            {
                case SFX_MAPUNIT_TWIP :
                {
					rPoint.setX( TWIPS_TO_MM( rPoint.getX() ) );
					rPoint.setY( TWIPS_TO_MM( rPoint.getY() ) );
                    break;
                }
                default:
                {
					DBG_ERROR("AW: Missing unit translation to 100th mm!");
                }
            }
        }
    }
}

//----------------------------------------------------------------------

void SvxShape::ForceMetricToItemPoolMetric(basegfx::B2DHomMatrix& rMatrix) const throw()
{
    DBG_TESTSOLARMUTEX();
	if(mpModel)
	{
		SfxMapUnit eMapUnit = mpModel->GetItemPool().GetMetric(0);
		if(eMapUnit != SFX_MAPUNIT_100TH_MM)
		{
			switch(eMapUnit)
			{
				case SFX_MAPUNIT_TWIP :
				{
			        basegfx::B2DTuple aScale;
			        basegfx::B2DTuple aTranslate;
			        double fRotate, fShearX;
        			
			        rMatrix.decompose(aScale, aTranslate, fRotate, fShearX);

                    aScale.setX(MM_TO_TWIPS(aScale.getX()));
                    aScale.setY(MM_TO_TWIPS(aScale.getY()));
                    aTranslate.setX(MM_TO_TWIPS(aTranslate.getX()));
                    aTranslate.setY(MM_TO_TWIPS(aTranslate.getY()));

                    rMatrix = basegfx::tools::createScaleShearXRotateTranslateB2DHomMatrix(
                        aScale, 
                        fShearX, 
                        fRotate, 
                        aTranslate);
                    break;
				}
				default:
				{
					DBG_ERROR("AW: Missing unit translation to PoolMetric!");
				}
			}
		}
	}
}

void SvxShape::ForceMetricTo100th_mm(basegfx::B2DHomMatrix& rMatrix) const throw()
{
    DBG_TESTSOLARMUTEX();
	SfxMapUnit eMapUnit = SFX_MAPUNIT_100TH_MM;
	if(mpModel)
	{
		eMapUnit = mpModel->GetItemPool().GetMetric(0);
		if(eMapUnit != SFX_MAPUNIT_100TH_MM)
		{
			switch(eMapUnit)
			{
				case SFX_MAPUNIT_TWIP :
				{
			        basegfx::B2DTuple aScale;
			        basegfx::B2DTuple aTranslate;
			        double fRotate, fShearX;
        			
			        rMatrix.decompose(aScale, aTranslate, fRotate, fShearX);

                    aScale.setX(TWIPS_TO_MM(aScale.getX()));
                    aScale.setY(TWIPS_TO_MM(aScale.getY()));
                    aTranslate.setX(TWIPS_TO_MM(aTranslate.getX()));
                    aTranslate.setY(TWIPS_TO_MM(aTranslate.getY()));

                    rMatrix = basegfx::tools::createScaleShearXRotateTranslateB2DHomMatrix(
                        aScale, 
                        fShearX, 
                        fRotate, 
                        aTranslate);
					break;
				}
				default:
				{
					DBG_ERROR("AW: Missing unit translation to 100th mm!");
				}
			}
		}
	}
}

//----------------------------------------------------------------------

void SvxShape::ForceMetricToItemPoolMetric(basegfx::B2DPolyPolygon& rPolyPolygon) const throw()
{
    DBG_TESTSOLARMUTEX();
	if(mpModel && rPolyPolygon.count())
	{
		SfxMapUnit eMapUnit = mpModel->GetItemPool().GetMetric(0);
		if(eMapUnit != SFX_MAPUNIT_100TH_MM)
		{
			switch(eMapUnit)
			{
				case SFX_MAPUNIT_TWIP :
				{
                    const double fFactorMmToTwips(72.0 / 127.0);
                    rPolyPolygon.transform(basegfx::tools::createScaleB2DHomMatrix(fFactorMmToTwips, fFactorMmToTwips));
					break;
				}
				default:
				{
					DBG_ERROR("AW: Missing unit translation to PoolMetric!");
				}
			}
		}
	}
}

void SvxShape::ForceMetricTo100th_mm(basegfx::B2DPolyPolygon& rPolyPolygon) const throw()
{
    DBG_TESTSOLARMUTEX();
    SfxMapUnit eMapUnit = SFX_MAPUNIT_100TH_MM;
	if(mpModel && rPolyPolygon.count())
    {
        eMapUnit = mpModel->GetItemPool().GetMetric(0);
        if(eMapUnit != SFX_MAPUNIT_100TH_MM)
        {
            switch(eMapUnit)
            {
                case SFX_MAPUNIT_TWIP :
                {
                    const double fFactorTwipsToMm(127.0 / 72.0);
                    rPolyPolygon.transform(basegfx::tools::createScaleB2DHomMatrix(fFactorTwipsToMm, fFactorTwipsToMm));
                    break;
                }
                default:
                {
					DBG_ERROR("AW: Missing unit translation to 100th mm!");
                }
            }
        }
    }
}

//----------------------------------------------------------------------

bool SvxShape::isWriterAnchorUsed() const
{
    return (mpModel
        && mpObj.is()
        && mpModel->IsWriter() 
        && !mpObj->GetAnchorPos().equalZero());
}

//----------------------------------------------------------------------
void SvxItemPropertySet_ObtainSettingsFromPropertySet(const SvxItemPropertySet& rPropSet,
  SfxItemSet& rSet, uno::Reference< beans::XPropertySet > xSet, const SfxItemPropertyMap* pMap )
{
	if(rPropSet.AreThereOwnUsrAnys())
	{
        const SfxItemPropertyMap* pSrc = rPropSet.getPropertyMap();
        PropertyEntryVector_t aSrcPropVector = pSrc->getPropertyEntries();
        PropertyEntryVector_t::const_iterator aSrcIt = aSrcPropVector.begin();
		while(aSrcIt != aSrcPropVector.end())
		{
			if(aSrcIt->nWID)
			{
				uno::Any* pUsrAny = rPropSet.GetUsrAnyForID(aSrcIt->nWID);
				if(pUsrAny)
				{
					// Aequivalenten Eintrag in pDst suchen
                    const SfxItemPropertySimpleEntry* pEntry = pMap->getByName( aSrcIt->sName );
					if(pEntry)
					{
						// entry found
						if(pEntry->nWID >= OWN_ATTR_VALUE_START && pEntry->nWID <= OWN_ATTR_VALUE_END)
						{
							// Special ID im PropertySet, kann nur direkt am
							// Objekt gesetzt werden+
							xSet->setPropertyValue( aSrcIt->sName, *pUsrAny);
						}
						else
						{
							if(rSet.GetPool()->IsWhich(pEntry->nWID))
								rSet.Put(rSet.GetPool()->GetDefaultItem(pEntry->nWID));
							// setzen
                            SvxItemPropertySet_setPropertyValue(rPropSet, pEntry, *pUsrAny, rSet);
						}
					}
				}
			}

			// next entry
			++aSrcIt;
		}
		const_cast< SvxItemPropertySet& >(rPropSet).ClearAllUsrAny();
	}
}


void SvxShape::ObtainSettingsFromPropertySet(const SvxItemPropertySet& rPropSet)
{
    DBG_TESTSOLARMUTEX();
	if(mpObj.is() && rPropSet.AreThereOwnUsrAnys() && mpModel)
	{
		SfxItemSet aSet( mpModel->GetItemPool(), SDRATTR_START, SDRATTR_END, 0);
		Reference< beans::XPropertySet > xShape( (OWeakObject*)this, UNO_QUERY );
		SvxItemPropertySet_ObtainSettingsFromPropertySet(rPropSet, aSet, xShape, mpPropSet->getPropertyMap() );

		mpObj->SetMergedItemSetAndBroadcast(aSet);

		mpObj->ApplyNotPersistAttr( aSet );
	}
}

//----------------------------------------------------------------------

uno::Any SvxShape::GetBitmap( sal_Bool bMetaFile /* = sal_False */ ) const throw()
{
    DBG_TESTSOLARMUTEX();
	uno::Any aAny;

	if( !mpObj.is() || mpModel == NULL || !mpObj->IsObjectInserted() || NULL == mpObj->getSdrPageFromSdrObject() )
		return aAny;

	VirtualDevice aVDev;
	aVDev.SetMapMode(MapMode(MAP_100TH_MM));

	SdrPage* pPage = mpObj->getSdrPageFromSdrObject();

	E3dView* pView = new E3dView( mpObj->getSdrModelFromSdrObject(), &aVDev );

	SdrObject *pTempObj = mpObj.get();
	pView->MarkObj(*pTempObj);

	GDIMetaFile aMtf( pView->GetMarkedObjMetaFile() );

    if( bMetaFile )
	{
		SvMemoryStream aDestStrm( 65535, 65535 );
		ConvertGDIMetaFileToWMF( aMtf, aDestStrm, NULL, sal_False );
        const uno::Sequence<sal_Int8> aSeq(
            static_cast< const sal_Int8* >(aDestStrm.GetData()),
            aDestStrm.GetEndOfData());
		aAny.setValue( &aSeq, ::getCppuType((const uno::Sequence< sal_Int8 >*)0) );
	}
	else
	{
    	const basegfx::B2DVector aObjectScale(basegfx::absolute(pTempObj->getSdrObjectScale()));
		Graphic aGraph(aMtf);

        aGraph.SetPrefSize(Size(basegfx::fround(aObjectScale.getX()), basegfx::fround(aObjectScale.getY())));
		aGraph.SetPrefMapMode(MAP_100TH_MM);

		Reference< awt::XBitmap > xBmp( aGraph.GetXGraphic(), UNO_QUERY );
		aAny <<= xBmp;
	}

	pView->UnmarkAll();
	delete pView;

	return aAny;
}

//----------------------------------------------------------------------

uno::Sequence< uno::Type > SAL_CALL SvxShape::getTypes()
	throw (uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		return mpImpl->mpMaster->getTypes();
	}
	else
	{
		return _getTypes();
	}
}

//----------------------------------------------------------------------

uno::Sequence< uno::Type > SAL_CALL SvxShape::_getTypes()
	throw(uno::RuntimeException)
{
	switch( mpImpl->mnObjId )
	{
	    // shapes without text
	    case OBJ_PAGE:
	    case OBJ_FRAME:
	    case OBJ_OLE2_PLUGIN:
	    case OBJ_OLE2_APPLET:
	    case E3D_CUBEOBJ_ID|E3D_INVENTOR_FLAG:
	    case E3D_SPHEREOBJ_ID|E3D_INVENTOR_FLAG:
	    case E3D_LATHEOBJ_ID|E3D_INVENTOR_FLAG:
	    case E3D_EXTRUDEOBJ_ID|E3D_INVENTOR_FLAG:
	    case E3D_POLYGONOBJ_ID|E3D_INVENTOR_FLAG:
	    case OBJ_MEDIA:
		{
			static ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > aTypeSequence;

			if( aTypeSequence.getLength() == 0 )
			{
				// Ready for multithreading; get global mutex for first call of this method only! see before
				MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

				// Control these pointer again ... it can be, that another instance will be faster then these!
				if( aTypeSequence.getLength() == 0 )
				{
					aTypeSequence.realloc( 12 );
					uno::Type* pTypes = aTypeSequence.getArray();

					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShape >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XComponent >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XMultiPropertySet >*)0);
//					*pTypes++ = ::getCppuType((const uno::Reference< beans::XTolerantMultiPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertyState >*)0);
					*pTypes++ = beans::XMultiPropertyStates::static_type();
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XGluePointsSupplier >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XChild >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XServiceInfo >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XTypeProvider >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XUnoTunnel >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XNamed >*)0);
				}
			}
			return aTypeSequence;
		}
    	// group shape
	    case OBJ_GRUP:
		{
			static ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > aTypeSequence;

			if( aTypeSequence.getLength() == 0 )
			{
				// Ready for multithreading; get global mutex for first call of this method only! see before
				MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

				// Control these pointer again ... it can be, that another instance will be faster then these!
				if( aTypeSequence.getLength() == 0 )
				{
					aTypeSequence.realloc( 14 );
					uno::Type* pTypes = aTypeSequence.getArray();

					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShape >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XComponent >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XMultiPropertySet >*)0);
//					*pTypes++ = ::getCppuType((const uno::Reference< beans::XTolerantMultiPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertyState >*)0);
					*pTypes++ = beans::XMultiPropertyStates::static_type();
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XGluePointsSupplier >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XChild >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XServiceInfo >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XTypeProvider >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XUnoTunnel >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XNamed >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShapes>*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShapeGroup>*)0);
				}
			}
			return aTypeSequence;
		}
    	// connector shape
	    case OBJ_EDGE:
		{
			static ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > aTypeSequence;

			if( aTypeSequence.getLength() == 0 )
			{
				// Ready for multithreading; get global mutex for first call of this method only! see before
				MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

				// Control these pointer again ... it can be, that another instance will be faster then these!
				if( aTypeSequence.getLength() == 0 )
				{
                    aTypeSequence.realloc( 17 );
					uno::Type* pTypes = aTypeSequence.getArray();

					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShape >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XComponent >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XMultiPropertySet >*)0);
//					*pTypes++ = ::getCppuType((const uno::Reference< beans::XTolerantMultiPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertyState >*)0);
					*pTypes++ = beans::XMultiPropertyStates::static_type();
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XGluePointsSupplier >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XChild >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XServiceInfo >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XTypeProvider >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XUnoTunnel >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XNamed >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XConnectorShape>*)0);
					// from SvxUnoTextBase::getTypes()
                    *pTypes++ = ::getCppuType(( const uno::Reference< text::XTextAppend >*)0);
                    *pTypes++ = ::getCppuType(( const uno::Reference< text::XTextCopy >*)0);
					*pTypes++ = ::getCppuType(( const uno::Reference< container::XEnumerationAccess >*)0);
					*pTypes++ = ::getCppuType(( const uno::Reference< text::XTextRangeMover >*)0);
				}
			}
			return aTypeSequence;
		}
    	// control shape
	    case OBJ_UNO:
		{
			static ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > aTypeSequence;

			if( aTypeSequence.getLength() == 0 )
			{
				// Ready for multithreading; get global mutex for first call of this method only! see before
				MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

				// Control these pointer again ... it can be, that another instance will be faster then these!
				if( aTypeSequence.getLength() == 0 )
				{
					aTypeSequence.realloc( 13 );
					uno::Type* pTypes = aTypeSequence.getArray();

					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShape >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XComponent >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XMultiPropertySet >*)0);
//					*pTypes++ = ::getCppuType((const uno::Reference< beans::XTolerantMultiPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertyState >*)0);
					*pTypes++ = beans::XMultiPropertyStates::static_type();
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XGluePointsSupplier >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XChild >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XServiceInfo >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XTypeProvider >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XUnoTunnel >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XNamed >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XControlShape>*)0);
				}
			}
			return aTypeSequence;
		}
    	// 3d scene shape
	    case E3D_SCENE_ID|E3D_INVENTOR_FLAG:
		{
			static ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > aTypeSequence;

			if( aTypeSequence.getLength() == 0 )
			{
				// Ready for multithreading; get global mutex for first call of this method only! see before
				MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

				// Control these pointer again ... it can be, that another instance will be faster then these!
				if( aTypeSequence.getLength() == 0 )
				{
					aTypeSequence.realloc( 13 );
					uno::Type* pTypes = aTypeSequence.getArray();

					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShape >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XComponent >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XMultiPropertySet >*)0);
//					*pTypes++ = ::getCppuType((const uno::Reference< beans::XTolerantMultiPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertyState >*)0);
					*pTypes++ = beans::XMultiPropertyStates::static_type();
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XGluePointsSupplier >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XChild >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XServiceInfo >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XTypeProvider >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XUnoTunnel >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XNamed >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShapes>*)0);
				}
			}
			return aTypeSequence;
		}
	    case OBJ_CUSTOMSHAPE:
		{
			static ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > aTypeSequence;

			if( aTypeSequence.getLength() == 0 )
			{
				// Ready for multithreading; get global mutex for first call of this method only! see before
				MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

				// Control these pointer again ... it can be, that another instance will be faster then these!
				if( aTypeSequence.getLength() == 0 )
				{
					aTypeSequence.realloc( 16 );
					uno::Type* pTypes = aTypeSequence.getArray();

					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShape >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XComponent >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XMultiPropertySet >*)0);
//					*pTypes++ = ::getCppuType((const uno::Reference< beans::XTolerantMultiPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertyState >*)0);
					*pTypes++ = beans::XMultiPropertyStates::static_type();
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XGluePointsSupplier >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XChild >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XServiceInfo >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XTypeProvider >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XUnoTunnel >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XNamed >*)0);
					// from SvxUnoTextBase::getTypes()
					*pTypes++ = ::getCppuType(( const uno::Reference< text::XText >*)0);
					*pTypes++ = ::getCppuType(( const uno::Reference< container::XEnumerationAccess >*)0);
					*pTypes++ = ::getCppuType(( const uno::Reference< text::XTextRangeMover >*)0);
					*pTypes++ = ::getCppuType(( const uno::Reference< drawing::XEnhancedCustomShapeDefaulter >*)0);
				}
			}
			return aTypeSequence;
		}
        // shapes with text
        case OBJ_RECT:
        case OBJ_CIRC:
        case OBJ_MEASURE:
        case OBJ_LINE:
        case OBJ_POLY:
        case OBJ_PLIN:
        case OBJ_PATHLINE:
        case OBJ_PATHFILL:
        case OBJ_FREELINE:
        case OBJ_FREEFILL:
        case OBJ_PATHPOLY:
        case OBJ_PATHPLIN:
        case OBJ_GRAF:
        case OBJ_TEXT:
        case OBJ_CAPTION:
        case OBJ_TABLE:
        case OBJ_OLE2: // #i118485# Moved to shapes with text, was at (shapes without text) before, see above
        default:
		{
			static ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > aTypeSequence;

			if( aTypeSequence.getLength() == 0 )
			{
				// Ready for multithreading; get global mutex for first call of this method only! see before
				MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

				// Control these pointer again ... it can be, that another instance will be faster then these!
				if( aTypeSequence.getLength() == 0 )
				{
                    aTypeSequence.realloc( 16 );
					uno::Type* pTypes = aTypeSequence.getArray();

					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XShape >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XComponent >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XMultiPropertySet >*)0);
//					*pTypes++ = ::getCppuType((const uno::Reference< beans::XTolerantMultiPropertySet >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< beans::XPropertyState >*)0);
					*pTypes++ = beans::XMultiPropertyStates::static_type();
					*pTypes++ = ::getCppuType((const uno::Reference< drawing::XGluePointsSupplier >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XChild >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XServiceInfo >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XTypeProvider >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< lang::XUnoTunnel >*)0);
					*pTypes++ = ::getCppuType((const uno::Reference< container::XNamed >*)0);
					// from SvxUnoTextBase::getTypes()
                    *pTypes++ = ::getCppuType(( const uno::Reference< text::XTextAppend >*)0);
                    *pTypes++ = ::getCppuType(( const uno::Reference< text::XTextCopy >*)0);
					*pTypes++ = ::getCppuType(( const uno::Reference< container::XEnumerationAccess >*)0);
					*pTypes++ = ::getCppuType(( const uno::Reference< text::XTextRangeMover >*)0);
				}
			}
			return aTypeSequence;
		}
	}
}

//----------------------------------------------------------------------

uno::Sequence< sal_Int8 > SAL_CALL SvxShape::getImplementationId()
	throw (uno::RuntimeException)
{
	static ::cppu::OImplementationId* pID = NULL ;

	if ( pID == NULL )
	{
		// Ready for multithreading; get global mutex for first call of this method only! see before
		MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

		// Control these pointer again ... it can be, that another instance will be faster then these!
		if ( pID == NULL )
		{
			// Create a new static ID ...
			static ::cppu::OImplementationId aID( sal_False ) ;
			// ... and set his address to static pointer!
			pID = &aID ;
		}
	}

	return pID->getImplementationId() ;
}

//----------------------------------------------------------------------

Reference< uno::XInterface > SvxShape_NewInstance()
{
	return uno::Reference< uno::XInterface >(static_cast< OWeakObject* >( new SvxShape() ) );
}

//----------------------------------------------------------------------
// SfxListener
//----------------------------------------------------------------------

void SvxShape::Notify( SfxBroadcaster&, const SfxHint& rHint ) throw()
{
    DBG_TESTSOLARMUTEX();
	if( !mpObj.is() )
		return;

    // #i55919# HINT_OBJCHG_* is only interesting if it's for this object
    const SdrBaseHint* pSdrHint = dynamic_cast< const SdrBaseHint* >(&rHint);

    if (!pSdrHint)
        return;

    if(HINT_MODELCLEARED != pSdrHint->GetSdrHintKind())
    {
        if(pSdrHint->GetSdrHintObject() != mpObj.get())
            return;

        if(HINT_OBJCHG_MOVE != pSdrHint->GetSdrHintKind()
            && HINT_OBJCHG_RESIZE != pSdrHint->GetSdrHintKind()
            && HINT_OBJCHG_ATTR != pSdrHint->GetSdrHintKind())
        {
        return;
        }
    }

	uno::Reference< uno::XInterface > xSelf( mpObj->getWeakUnoShape() );
	if( !xSelf.is() )
	{
		mpObj.reset( NULL );
		return;
	}

	sal_Bool bClearMe = sal_False;

	switch( pSdrHint->GetSdrHintKind() )
	{
		case HINT_OBJCHG_MOVE:
		case HINT_OBJCHG_RESIZE:
		case HINT_OBJCHG_ATTR:
		{
			updateShapeKind();
			break;
		}
		case HINT_MODELCLEARED:
		{
			bClearMe = sal_True;
			mpModel = NULL;
			break;
		}
		default:
			break;
	};

	if( bClearMe )
	{
		if( !HasSdrObjectOwnership() )
			mpObj.reset( NULL );
		if ( !mpImpl->mbDisposing )
			dispose();
	}
}

// XShape

//----------------------------------------------------------------------
// The "*LogicRectHack" functions also existed in sch, and those
// duplicate symbols cause Bad Things To Happen (TM)  #i9462#.
// Prefixing with 'svx' and marking static to make sure name collisions
// do not occur.

static bool svx_needLogicRangeHack( SdrObject* pObj )
{
	if( pObj->GetObjInventor() == SdrInventor)
	{
		switch(pObj->GetObjIdentifier())
		{
		    case OBJ_GRUP:
		    case OBJ_LINE:
		    case OBJ_POLY:
		    case OBJ_PLIN:
		    case OBJ_PATHLINE:
		    case OBJ_PATHFILL:
		    case OBJ_FREELINE:
		    case OBJ_FREEFILL:
		    case OBJ_EDGE:
		    case OBJ_PATHPOLY:
		    case OBJ_PATHPLIN:
		    case OBJ_MEASURE:
			    return sal_True;
		}
	}
	return sal_False;
}

//----------------------------------------------------------------------

static basegfx::B2DRange svx_getLogicRangeHack( SdrObject* pObj )
{
	if(svx_needLogicRangeHack(pObj))
	{
		return sdr::legacy::GetSnapRange(*pObj);
	}
	else
	{
		return sdr::legacy::GetLogicRange(*pObj);
	}
}

//----------------------------------------------------------------------

static void svx_setLogicRangeHack( SdrObject* pObj, const basegfx::B2DRange& rRange ) // TTTT needed?
{
	if(svx_needLogicRangeHack(pObj))
	{
		sdr::legacy::SetSnapRange(*pObj, rRange );
	}
	else
	{
		sdr::legacy::SetLogicRange(*pObj, rRange );
	}
}

//----------------------------------------------------------------------

awt::Point SAL_CALL SvxShape::getPosition() throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mpObj.is() && mpModel)
	{
		const basegfx::B2DRange aRange( svx_getLogicRangeHack(mpObj.get()) );
		basegfx::B2DPoint aPt( aRange.getMinimum() );

		// Position is relativ to anchor, so recalc to absolut position
        if(isWriterAnchorUsed())
        {
			aPt -= mpObj->GetAnchorPos();
        }

		ForceMetricTo100th_mm(aPt);
		return ::com::sun::star::awt::Point( basegfx::fround(aPt.getX()), basegfx::fround(aPt.getY()) );
	}
	else
	{
		return maPosition;
	}
}

//----------------------------------------------------------------------
void SAL_CALL SvxShape::setPosition( const awt::Point& Position ) throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mpObj.is() && mpModel )
	{
		// do NOT move 3D objects, this would change the homogen
		// transformation matrix
		if(!dynamic_cast< E3dCompoundObject* >(mpObj.get()))
		{
			const basegfx::B2DRange aRange( svx_getLogicRangeHack(mpObj.get()) );
			basegfx::B2DPoint aLocalPos( Position.X, Position.Y );
			ForceMetricToItemPoolMetric(aLocalPos);

			// Position ist absolut, relativ zum Anker stellen
            if(isWriterAnchorUsed())
            {
				aLocalPos += mpObj->GetAnchorPos();
            }

			sdr::legacy::transformSdrObject(*mpObj.get(), basegfx::tools::createTranslateB2DHomMatrix(aLocalPos - aRange.getMinimum()));
			mpModel->SetChanged();
		}
	}

	maPosition = Position;
}

//----------------------------------------------------------------------
awt::Size SAL_CALL SvxShape::getSize() throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mpObj.is() && mpModel)
	{
		const basegfx::B2DRange aRange( svx_getLogicRangeHack(mpObj.get()) );
		basegfx::B2DPoint aObjSize( aRange.getRange() );
		ForceMetricTo100th_mm(aObjSize);
		return ::com::sun::star::awt::Size( basegfx::fround(aObjSize.getX()), basegfx::fround(aObjSize.getY()) );
	}
	else
		return maSize;
}

//----------------------------------------------------------------------
void SAL_CALL SvxShape::setSize( const awt::Size& rSize )
	throw(beans::PropertyVetoException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mpObj.is() && mpModel)
	{
		basegfx::B2DRange aRange( svx_getLogicRangeHack(mpObj.get()) );
		basegfx::B2DPoint aLocalSize( rSize.Width, rSize.Height );
		ForceMetricToItemPoolMetric(aLocalSize);

		if(SdrInventor == mpObj->GetObjInventor() && OBJ_MEASURE == mpObj->GetObjIdentifier())
        {
			const basegfx::B2DVector aFactor(aLocalSize.getX() / aRange.getWidth(), aLocalSize.getY() / aRange.getHeight());
			const basegfx::B2DPoint aPt(sdr::legacy::GetSnapRange(*mpObj.get()).getMinimum());
			basegfx::B2DHomMatrix aTransform;

			aTransform.translate(-aPt);
			aTransform.scale(aFactor);
			aTransform.translate(aPt);

			sdr::legacy::transformSdrObject(*mpObj.get(), aTransform);
        }
        else
        {
            //aRange.SetSize(aLocalSize); // this call substract 1 // http://www.openoffice.org/issues/show_bug.cgi?id=83193
			aRange = basegfx::B2DRange(
				aRange.getMinX(), aRange.getMinY(), 
				aRange.getMinX() + aLocalSize.getX(), aRange.getMinY() + aLocalSize.getY());

			svx_setLogicRangeHack( mpObj.get(), aRange );
		}

		mpModel->SetChanged();
	}
	maSize = rSize;
}

//----------------------------------------------------------------------

// XNamed
OUString SAL_CALL SvxShape::getName(  ) throw(::com::sun::star::uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );
	if( mpObj.is() )
	{
		return mpObj->GetName();
	}
	else
	{
		return maShapeName;
	}
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::setName( const ::rtl::OUString& aName ) throw(::com::sun::star::uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );
	if( mpObj.is() )
	{
		mpObj->SetName( aName );
	}
	else
	{
		maShapeName = aName;
	}
}

// XShapeDescriptor

//----------------------------------------------------------------------
OUString SAL_CALL SvxShape::getShapeType() throw(uno::RuntimeException)
{
	if( 0 == maShapeType.getLength() )
	{
		UHashMapEntry* pMap = pSdrShapeIdentifierMap;
		while ( ( pMap->nId != mpImpl->mnObjId ) && pMap->aIdentifier.getLength() )
			++pMap;

		if ( pMap->aIdentifier.getLength() )
		{
			return pMap->aIdentifier;
		}
		else
		{
			DBG_ERROR("[CL] unknown SdrObjekt identifier");
		}
	}

	return maShapeType;
}

// XComponent

//----------------------------------------------------------------------
void SAL_CALL SvxShape::dispose() throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mpImpl->mbDisposing )
		return;	// caught a recursion

	mpImpl->mbDisposing = true;

	lang::EventObject aEvt;
	aEvt.Source = *(OWeakAggObject*) this;
	mpImpl->maDisposeListeners.disposeAndClear(aEvt);
    mpImpl->maPropertyNotifier.disposing();

	if ( mpObj.is() )
    {
        bool bFreeSdrObject = false;

        if ( mpObj->IsObjectInserted() && mpObj->getSdrPageFromSdrObject() )
	    {
            OSL_ENSURE( HasSdrObjectOwnership(), "SvxShape::dispose: is the below code correct?" );
                // normally, we are allowed to free the SdrObject only if we have its ownership.
                // Why isn't this checked here?

		    SdrPage* pPage = mpObj->getSdrPageFromSdrObject();
		    // SdrObject aus der Page loeschen
		    sal_uInt32 nCount = pPage->GetObjCount();
		    for ( sal_uInt32 nNum = 0; nNum < nCount; ++nNum )
		    {
			    if ( pPage->GetObj( nNum ) == mpObj.get() )
			    {
                    OSL_VERIFY( pPage->RemoveObjectFromSdrObjList( nNum ) == mpObj.get() );
                    bFreeSdrObject = true;
				    break;
			    }
		    }
	    }

        SetUnoShapeAtSdrObjectFromSvxShape(*mpObj.get(), 0);

        if ( bFreeSdrObject )
        {
            // in case we have the ownership of the SdrObject, a Free
            // would do nothing. So ensure the ownership is reset.
            mpImpl->mbHasSdrObjectOwnership = false;
            SdrObject* pObject = mpObj.get();
            deleteSdrObjectSafeAndClearPointer( pObject );
        }
    }

	if( mpModel )
	{
		EndListening( *mpModel );
		mpModel = NULL;
	}
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::addEventListener( const Reference< lang::XEventListener >& xListener )
	throw(uno::RuntimeException)
{
	mpImpl->maDisposeListeners.addInterface(xListener);
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::removeEventListener( const Reference< lang::XEventListener >& aListener ) throw(uno::RuntimeException)
{
   mpImpl->maDisposeListeners.removeInterface(aListener);
}

// XPropertySet

//----------------------------------------------------------------------

Reference< beans::XPropertySetInfo > SAL_CALL
	SvxShape::getPropertySetInfo() throw(uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		return mpImpl->mpMaster->getPropertySetInfo();
	}
	else
	{
		return _getPropertySetInfo();
	}
}

Reference< beans::XPropertySetInfo > SAL_CALL
	SvxShape::_getPropertySetInfo() throw(uno::RuntimeException)
{
	return mpPropSet->getPropertySetInfo();
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::addPropertyChangeListener( const OUString& _propertyName, const Reference< beans::XPropertyChangeListener >& _listener  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
    ::osl::MutexGuard aGuard( maMutex );
    mpImpl->maPropertyNotifier.addPropertyChangeListener( _propertyName, _listener );
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::removePropertyChangeListener( const OUString& _propertyName, const Reference< beans::XPropertyChangeListener >& _listener  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
    ::osl::MutexGuard aGuard( maMutex );
    mpImpl->maPropertyNotifier.removePropertyChangeListener( _propertyName, _listener );
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::addVetoableChangeListener( const OUString& , const Reference< beans::XVetoableChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
    OSL_ENSURE( false, "SvxShape::addVetoableChangeListener: don't have any vetoable properties, so why ...?" );
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::removeVetoableChangeListener( const OUString& , const Reference< beans::XVetoableChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
    OSL_ENSURE( false, "SvxShape::removeVetoableChangeListener: don't have any vetoable properties, so why ...?" );
}

//----------------------------------------------------------------------

sal_Bool SAL_CALL SvxShape::SetFillAttribute( sal_Int32 nWID, const OUString& rName )
{
	if(mpModel)
	{
    	SfxItemSet aSet( mpModel->GetItemPool(),	(sal_uInt16)nWID, (sal_uInt16)nWID );

	    if( SetFillAttribute( nWID, rName, aSet, mpModel ) )
	    {
		    mpObj->SetMergedItemSetAndBroadcast(aSet);

		    return sal_True;
	    }
	}

	return sal_False;
}

//----------------------------------------------------------------------

sal_Bool SAL_CALL SvxShape::SetFillAttribute( sal_Int32 nWID, const ::rtl::OUString& rName, SfxItemSet& rSet, SdrModel* pModel )
{
	// check if an item with the given name and which id is inside the models
	// pool or the stylesheet pool, if found its puttet in the itemse
	if( !SetFillAttribute( nWID, rName, rSet ) )
	{
		// we did not find such item in one of the pools, so we check
		// the property lists that are loaded for the model for items
		// that support such.
		String aStrName;
		SvxUnogetInternalNameForItem( (sal_Int16)nWID, rName, aStrName );

		switch( nWID )
		{
		    case XATTR_FILLBITMAP:
		    {
			    XBitmapList* pBitmapList = pModel->GetBitmapList();

                if( !pBitmapList )
                    return sal_False;

                long nPos = ((XPropertyList*)pBitmapList)->Get(aStrName);
                if( nPos == -1 )
                    return sal_False;

                XBitmapEntry* pEntry = pBitmapList->GetBitmap( nPos );
                XFillBitmapItem aBmpItem;
                aBmpItem.SetWhich( XATTR_FILLBITMAP );
                aBmpItem.SetName( rName );
                aBmpItem.SetGraphicObject(pEntry->GetGraphicObject());
                rSet.Put( aBmpItem );
			    break;
		    }
		    case XATTR_FILLGRADIENT:
		    {
			    XGradientList* pGradientList = pModel->GetGradientList();

                if( !pGradientList )
                    return sal_False;

                long nPos = ((XPropertyList*)pGradientList)->Get(aStrName);
                if( nPos == -1 )
                    return sal_False;

                XGradientEntry* pEntry = pGradientList->GetGradient( nPos );
                XFillGradientItem aGrdItem;
                aGrdItem.SetWhich( XATTR_FILLGRADIENT );
                aGrdItem.SetName( rName );
                aGrdItem.SetGradientValue( pEntry->GetGradient() );
                rSet.Put( aGrdItem );
                break;
		    }
		    case XATTR_FILLHATCH:
		    {
			    XHatchList* pHatchList = pModel->GetHatchList();

                if( !pHatchList )
                    return sal_False;

			    long nPos = ((XPropertyList*)pHatchList)->Get(aStrName);
			    if( nPos == -1 )
				    return sal_False;

			    XHatchEntry* pEntry = pHatchList->GetHatch( nPos );
			    XFillHatchItem aHatchItem;
			    aHatchItem.SetWhich( XATTR_FILLHATCH );
			    aHatchItem.SetName( rName );
			    aHatchItem.SetHatchValue( pEntry->GetHatch() );
			    rSet.Put( aHatchItem );
			    break;
		    }
		    case XATTR_LINEEND:
		    case XATTR_LINESTART:
		    {
			    XLineEndList* pLineEndList = pModel->GetLineEndList();

                if( !pLineEndList )
                    return sal_False;

			    long nPos = ((XPropertyList*)pLineEndList)->Get(aStrName);
			    if( nPos == -1 )
				    return sal_False;

			    XLineEndEntry* pEntry = pLineEndList->GetLineEnd( nPos );
			    if( XATTR_LINEEND == nWID )
			    {
				    XLineEndItem aLEItem;
				    aLEItem.SetWhich( XATTR_LINEEND );
				    aLEItem.SetName( rName );
				    aLEItem.SetLineEndValue( pEntry->GetLineEnd() );
				    rSet.Put( aLEItem );
			    }
			    else
			    {
				    XLineStartItem aLSItem;
				    aLSItem.SetWhich( XATTR_LINESTART );
				    aLSItem.SetName( rName );
				    aLSItem.SetLineStartValue( pEntry->GetLineEnd() );
				    rSet.Put( aLSItem );
			    }

			    break;
		    }
		    case XATTR_LINEDASH:
		    {
			    XDashList* pDashList = pModel->GetDashList();

                if( !pDashList )
                    return sal_False;

			    long nPos = ((XPropertyList*)pDashList)->Get(aStrName);
			    if( nPos == -1 )
				    return sal_False;

			    XDashEntry* pEntry = pDashList->GetDash( nPos );
			    XLineDashItem aDashItem;
			    aDashItem.SetWhich( XATTR_LINEDASH );
			    aDashItem.SetName( rName );
			    aDashItem.SetDashValue( pEntry->GetDash() );
			    rSet.Put( aDashItem );
			    break;
		    }
		    default:
            {
			    return sal_False;
		    }
    	}
	}

	return sal_True;
}

//----------------------------------------------------------------------

sal_Bool SAL_CALL SvxShape::SetFillAttribute( sal_Int32 nWID, const OUString& rName, SfxItemSet& rSet )
{
	String aName;
	SvxUnogetInternalNameForItem( (sal_Int16)nWID, rName, aName );

	if( aName.Len() == 0 )
	{
		switch( nWID )
		{
		    case XATTR_LINEEND:
		    case XATTR_LINESTART:
			{
				const String aEmpty;
				const basegfx::B2DPolyPolygon aEmptyPoly;
				if( nWID == XATTR_LINEEND )
					rSet.Put( XLineEndItem( aEmpty, aEmptyPoly ) );
				else
					rSet.Put( XLineStartItem( aEmpty, aEmptyPoly ) );

				return sal_True;
			}
    		case XATTR_FILLFLOATTRANSPARENCE:
			{
				// #85953# Set a disabled XFillFloatTransparenceItem
				rSet.Put(XFillFloatTransparenceItem());

				return sal_True;
			}
		}

		return sal_False;
	}

	const SfxItemPool* pPool = rSet.GetPool();
	const String aSearchName( aName );
	const sal_uInt32 nCount = pPool->GetItemCount2((sal_uInt16)nWID);
	const NameOrIndex* pItem;

	for( sal_uInt32 nSurrogate = 0; nSurrogate < nCount; nSurrogate++ )
	{
		pItem = (NameOrIndex*)pPool->GetItem2((sal_uInt16)nWID, nSurrogate);
		if( pItem && ( pItem->GetName() == aSearchName ) )
		{
			rSet.Put( *pItem );
			return sal_True;
		}
	}

	return sal_False;
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::setPropertyValue( const OUString& rPropertyName, const uno::Any& rVal )
	throw(beans::UnknownPropertyException, beans::PropertyVetoException, lang::IllegalArgumentException, lang::WrappedTargetException, uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		mpImpl->mpMaster->setPropertyValue( rPropertyName, rVal );
	}
	else
	{
		_setPropertyValue( rPropertyName, rVal );
	}
}

void SAL_CALL SvxShape::_setPropertyValue( const OUString& rPropertyName, const uno::Any& rVal )
	throw(beans::UnknownPropertyException, beans::PropertyVetoException, lang::IllegalArgumentException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

    const SfxItemPropertySimpleEntry* pMap = mpPropSet->getPropertyMapEntry(rPropertyName);

	if( mpObj.is() && mpModel )
	{
		if( pMap == NULL )
			throw beans::UnknownPropertyException();

		if( (pMap->nFlags & beans::PropertyAttribute::READONLY ) != 0 )
			throw beans::PropertyVetoException();

		mpModel->SetChanged();

        if(!setPropertyValueImpl( rPropertyName, pMap, rVal ) )
		{
			DBG_ASSERT( pMap->nWID == SDRATTR_TEXTDIRECTION || pMap->nWID < SDRATTR_NOTPERSIST_FIRST || pMap->nWID > SDRATTR_NOTPERSIST_LAST, "Not persist item not handled!" );
			DBG_ASSERT( pMap->nWID < OWN_ATTR_VALUE_START || pMap->nWID > OWN_ATTR_VALUE_END, "Not item property not handled!" );

			sal_Bool bIsNotPersist = pMap->nWID >= SDRATTR_NOTPERSIST_FIRST && pMap->nWID <= SDRATTR_NOTPERSIST_LAST && pMap->nWID != SDRATTR_TEXTDIRECTION;

			if( pMap->nWID == SDRATTR_ECKENRADIUS )
			{
				sal_Int32 nCornerRadius = 0;
				if( !(rVal >>= nCornerRadius) || (nCornerRadius < 0) || (nCornerRadius > 5000000))
					throw IllegalArgumentException();
			}

			SfxItemSet* pSet;
			if( mbIsMultiPropertyCall && !bIsNotPersist )
			{
				if( mpImpl->mpItemSet == NULL )
				{
					pSet = mpImpl->mpItemSet = mpObj->GetMergedItemSet().Clone();
				}
				else
				{
					pSet = mpImpl->mpItemSet;
				}
			}
			else
			{
				pSet = new SfxItemSet( mpModel->GetItemPool(),	pMap->nWID, pMap->nWID);
			}

			if( pSet->GetItemState( pMap->nWID ) != SFX_ITEM_SET )
				pSet->Put(mpObj->GetMergedItem(pMap->nWID));

			if( !SvxUnoTextRangeBase::SetPropertyValueHelper( *pSet, pMap, rVal, *pSet ))
			{
				if( pSet->GetItemState( pMap->nWID ) != SFX_ITEM_SET )
				{
					if(bIsNotPersist)
					{
						// Not-Persistant Attribute, hole diese extra
						mpObj->TakeNotPersistAttr(*pSet, sal_False);
					}
				}

				if( pSet->GetItemState( pMap->nWID ) != SFX_ITEM_SET )
				{
					// Default aus ItemPool holen
					if(mpModel->GetItemPool().IsWhich(pMap->nWID))
						pSet->Put(mpModel->GetItemPool().GetDefaultItem(pMap->nWID));
				}

				if( pSet->GetItemState( pMap->nWID ) == SFX_ITEM_SET )
				{
					SvxItemPropertySet_setPropertyValue( *mpPropSet, pMap, rVal, *pSet );
				}
			}

			if(bIsNotPersist)
			{
				// Not-Persist Attribute extra setzen
				mpObj->ApplyNotPersistAttr( *pSet );
				delete pSet;
			}
			else
			{
				// if we have a XMultiProperty call then the item set
				// will be set in setPropertyValues later
				if( !mbIsMultiPropertyCall )
				{
					mpObj->SetMergedItemSetAndBroadcast( *pSet );

					delete pSet;
				}
			}
			return;
		}
	}
	else
	{
		// since we have no actual sdr object right now
		// remember all properties in a list. These
		// properties will be set when the sdr object is
		// created

		if(pMap && pMap->nWID)
// Fixme: We should throw a UnknownPropertyException here.
//		  But since this class is aggregated from classes
//		  that support additional properties that we don't
//		  know here we silently store *all* properties, even
//		  if they may be not supported after creation
			mpPropSet->setPropertyValue( pMap, rVal );
	}
}

//----------------------------------------------------------------------

uno::Any SAL_CALL SvxShape::getPropertyValue( const OUString& PropertyName )
	throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
	if ( mpImpl->mpMaster )
		return mpImpl->mpMaster->getPropertyValue( PropertyName );
	else
		return _getPropertyValue( PropertyName );
}

//----------------------------------------------------------------------

uno::Any SvxShape::_getPropertyValue( const OUString& PropertyName )
	throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

    const SfxItemPropertySimpleEntry* pMap = mpPropSet->getPropertyMapEntry(PropertyName);

	uno::Any aAny;
	if( mpObj.is() && mpModel )
	{
		if(pMap == NULL )
			throw beans::UnknownPropertyException();

        if( !getPropertyValueImpl( PropertyName, pMap, aAny ) )
		{
			DBG_ASSERT( pMap->nWID == SDRATTR_TEXTDIRECTION || (pMap->nWID < SDRATTR_NOTPERSIST_FIRST || pMap->nWID > SDRATTR_NOTPERSIST_LAST), "Not persist item not handled!" );
			DBG_ASSERT( pMap->nWID < OWN_ATTR_VALUE_START || pMap->nWID > OWN_ATTR_VALUE_END, "Not item property not handled!" );

			SfxItemSet aSet( mpModel->GetItemPool(),	pMap->nWID, pMap->nWID);
			aSet.Put(mpObj->GetMergedItem(pMap->nWID));

			if(SvxUnoTextRangeBase::GetPropertyValueHelper(  aSet, pMap, aAny ))
				return aAny;

			if(!aSet.Count())
			{
				if(pMap->nWID >= SDRATTR_NOTPERSIST_FIRST && pMap->nWID <= SDRATTR_NOTPERSIST_LAST)
				{
					// Not-Persistant Attribute, hole diese extra
					mpObj->TakeNotPersistAttr(aSet, sal_False);
				}
			}

			if(!aSet.Count())
			{
				// Default aus ItemPool holen
				if(mpModel->GetItemPool().IsWhich(pMap->nWID))
					aSet.Put(mpModel->GetItemPool().GetDefaultItem(pMap->nWID));
			}

			if(aSet.Count())
				aAny = GetAnyForItem( aSet, pMap );
		}
	}
	else
	{

// Fixme: we should	return default values for OWN_ATTR !

		if(pMap && pMap->nWID)
//		FixMe: see setPropertyValue
			aAny = mpPropSet->getPropertyValue( pMap );

	}
	return aAny;
}

//----------------------------------------------------------------------

// XMultiPropertySet
void SAL_CALL SvxShape::setPropertyValues( const ::com::sun::star::uno::Sequence< ::rtl::OUString >& aPropertyNames, const ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Any >& aValues ) throw (::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException)
{
	OGuard aSolarGuard( Application::GetSolarMutex() );

    const sal_Int32 nCount = aPropertyNames.getLength();
	const OUString* pNames = aPropertyNames.getConstArray();

	const uno::Any* pValues = aValues.getConstArray();

	// make sure mbIsMultiPropertyCall and mpImpl->mpItemSet are
	// reseted even when an execption is thrown
    const ::comphelper::ScopeGuard aGuard( boost::bind( &SvxShape::endSetPropertyValues, this ) );

	mbIsMultiPropertyCall = sal_True;

	if( mpImpl->mpMaster )
	{
		for( sal_Int32 nIdx = 0; nIdx < nCount; nIdx++, pNames++, pValues++ )
		{
			try
			{
				setPropertyValue( *pNames, *pValues );
			}
			catch( beans::UnknownPropertyException& e )
			{
				(void)e;
			}
            catch( uno::Exception& ex )
            {
				(void)ex;
            }
		}
	}
	else
	{
		uno::Reference< beans::XPropertySet > xSet;
		queryInterface( ::getCppuType( (const uno::Reference< beans::XPropertySet >*) 0) ) >>= xSet;

		for( sal_Int32 nIdx = 0; nIdx < nCount; nIdx++, pNames++, pValues++ )
		{
			try
			{
				xSet->setPropertyValue( *pNames, *pValues );
			}
			catch( beans::UnknownPropertyException& e )
			{
				(void)e;
			}
            catch( uno::Exception& ex )
            {
				(void)ex;
            }
		}
	}

	if( mpImpl->mpItemSet && mpObj.is() )
		mpObj->SetMergedItemSetAndBroadcast( *mpImpl->mpItemSet );
}

//----------------------------------------------------------------------

void SvxShape::endSetPropertyValues()
{
	mbIsMultiPropertyCall = sal_False;
	if( mpImpl->mpItemSet )
	{
		delete mpImpl->mpItemSet;
		mpImpl->mpItemSet = 0;
	}
}

//----------------------------------------------------------------------

::com::sun::star::uno::Sequence< ::com::sun::star::uno::Any > SAL_CALL SvxShape::getPropertyValues( const ::com::sun::star::uno::Sequence< ::rtl::OUString >& aPropertyNames ) throw (::com::sun::star::uno::RuntimeException)
{
	const sal_Int32 nCount = aPropertyNames.getLength();
	const OUString* pNames = aPropertyNames.getConstArray();

	uno::Sequence< uno::Any > aRet( nCount );
	uno::Any* pValue = aRet.getArray();;

	if( mpImpl->mpMaster )
	{
		for( sal_Int32 nIdx = 0; nIdx < nCount; nIdx++, pValue++, pNames++ )
		{
			try
			{
				*pValue = getPropertyValue( *pNames );
			}
			catch( uno::Exception& )
			{
				DBG_ERROR( "SvxShape::getPropertyValues, unknown property asked" );
			}
		}
	}
	else
	{
		uno::Reference< beans::XPropertySet > xSet;
		queryInterface( ::getCppuType( (const uno::Reference< beans::XPropertySet >*) 0) ) >>= xSet;

		for( sal_Int32 nIdx = 0; nIdx < nCount; nIdx++, pValue++, pNames++ )
		{
			try
			{
				*pValue = xSet->getPropertyValue( *pNames );
			}
			catch( uno::Exception& )
			{
				DBG_ERROR( "SvxShape::getPropertyValues, unknown property asked" );
			}
		}
	}

	return aRet;
}

void SAL_CALL SvxShape::addPropertiesChangeListener( const ::com::sun::star::uno::Sequence< ::rtl::OUString >& , const ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertiesChangeListener >&  ) throw (::com::sun::star::uno::RuntimeException)
{
}

void SAL_CALL SvxShape::removePropertiesChangeListener( const ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertiesChangeListener >&  ) throw (::com::sun::star::uno::RuntimeException)
{
}

void SAL_CALL SvxShape::firePropertiesChangeEvent( const ::com::sun::star::uno::Sequence< ::rtl::OUString >& , const ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertiesChangeListener >&  ) throw (::com::sun::star::uno::RuntimeException)
{
}

//----------------------------------------------------------------------

uno::Any SvxShape::GetAnyForItem( SfxItemSet& aSet, const SfxItemPropertySimpleEntry* pMap ) const
{
    DBG_TESTSOLARMUTEX();

	// Hole Wert aus ItemSet
	uno::Any aAny = SvxItemPropertySet_getPropertyValue( *mpPropSet, pMap, aSet );

	if( *pMap->pType != aAny.getValueType() )
	{
		// since the sfx uint16 item now exports a sal_Int32, we may have to fix this here
		if( ( *pMap->pType == ::getCppuType((const sal_Int16*)0)) && aAny.getValueType() == ::getCppuType((const sal_Int32*)0) )
		{
			sal_Int32 nValue = 0;
			aAny >>= nValue;
			aAny <<= (sal_Int16)nValue;
		}
		else
		{
			DBG_ERROR("SvxShape::GetAnyForItem() Returnvalue has wrong Type!" );
		}
	}

	return aAny;
}

//----------------------------------------------------------------------

// XPropertyState
beans::PropertyState SAL_CALL SvxShape::getPropertyState( const OUString& PropertyName )
	throw(beans::UnknownPropertyException, uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		return mpImpl->mpMaster->getPropertyState( PropertyName );
	}
	else
	{
		return _getPropertyState( PropertyName );
	}
}

beans::PropertyState SAL_CALL SvxShape::_getPropertyState( const OUString& PropertyName )
	throw(beans::UnknownPropertyException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

    const SfxItemPropertySimpleEntry* pMap = mpPropSet->getPropertyMapEntry(PropertyName);

	if( !mpObj.is() || pMap == NULL )
		throw beans::UnknownPropertyException();

	beans::PropertyState eState;
	if( !getPropertyStateImpl( pMap, eState ) )
	{
		const SfxItemSet& rSet = mpObj->GetMergedItemSet();

		switch( rSet.GetItemState( pMap->nWID, sal_False ) )
		{
		    case SFX_ITEM_READONLY:
		    case SFX_ITEM_SET:
			    eState = beans::PropertyState_DIRECT_VALUE;
			    break;
		    case SFX_ITEM_DEFAULT:
			    eState = beans::PropertyState_DEFAULT_VALUE;
			    break;
    //		case SFX_ITEM_UNKNOWN:
    //		case SFX_ITEM_DONTCARE:
    //		case SFX_ITEM_DISABLED:
		    default:
			    eState = beans::PropertyState_AMBIGUOUS_VALUE;
			    break;
		}

		// if a item is set, this doesn't mean we want it :)
		if( ( beans::PropertyState_DIRECT_VALUE == eState ) )
		{
			switch( pMap->nWID )
			{
			    // the following items are disabled by changing the
			    // fill style or the line style. so there is no need
			    // to export items without names which should be empty
			    case XATTR_FILLBITMAP:
			    case XATTR_FILLGRADIENT:
			    case XATTR_FILLHATCH:
			    case XATTR_LINEDASH:
				{
					NameOrIndex* pItem = (NameOrIndex*)rSet.GetItem((sal_uInt16)pMap->nWID);
					if( ( pItem == NULL ) || ( pItem->GetName().Len() == 0) )
						eState = beans::PropertyState_DEFAULT_VALUE;
				}
				break;

			    // #i36115#
			    // If e.g. the LineStart is on NONE and thus the string has length 0, it still
			    // may be a hard attribute covering the set LineStart of the parent (Style).
			    // #i37644#
			    // same is for fill float transparency
			    case XATTR_LINEEND:
			    case XATTR_LINESTART:
			    case XATTR_FILLFLOATTRANSPARENCE:
				{
					NameOrIndex* pItem = (NameOrIndex*)rSet.GetItem((sal_uInt16)pMap->nWID);
					if( ( pItem == NULL ) )
						eState = beans::PropertyState_DEFAULT_VALUE;
				}
				break;
			}
		}
	}
	return eState;
}

//----------------------------------------------------------------------

bool SvxShape::setPropertyValueImpl( const ::rtl::OUString&, const SfxItemPropertySimpleEntry* pProperty, const ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException)
{
	switch( pProperty->nWID )
	{
	    case OWN_ATTR_CAPTION_POINT:
	    {
		    awt::Point aPnt;
		    if( rValue >>= aPnt )
		    {
                // prepare geometry data
			    basegfx::B2DPoint aVclPoint( aPnt.X, aPnt.Y );

			    // #88657# metric of pool maybe twips (writer)
			    ForceMetricToItemPoolMetric(aVclPoint);

			    // #88491# position relative to anchor
			    // Not needed anymore, pos is already relative to object's
			    // TopLeft, anchor is not relevant for relative position
	            //if(isWriterAnchorUsed())
			    //{
			    //    aVclPoint += sdr::legacy::GetAnchorPos(*mpObj.get());
			    //}

                // #90763# position is relative to top left, make it absolute
			    aVclPoint += mpObj->getSdrObjectTranslate();
//			    const basegfx::B2DHomMatrix aObjectMatrix(mpObj->getSdrObjectTransformation());
//			    aVclPoint.X() += basegfx::fround(aObjectMatrix.get(0, 2));
//			    aVclPoint.Y() += basegfx::fround(aObjectMatrix.get(1, 2));

			    dynamic_cast< SdrCaptionObj* >(mpObj.get())->SetTailPos(aVclPoint);

			    return true;
		    }
		    break;
	    }
	    case OWN_ATTR_TRANSFORMATION:
	    {
		    drawing::HomogenMatrix3 aMatrix;
		    if(rValue >>= aMatrix)
		    {
			    basegfx::B2DHomMatrix aNewObjectMatrix(basegfx::tools::UnoHomogenMatrix3ToB2DHomMatrix(aMatrix));

			    // metric of pool maybe twips (writer)
			    ForceMetricToItemPoolMetric(aNewObjectMatrix);

                // position relative to anchor
	            if(isWriterAnchorUsed())
                {
                    aNewObjectMatrix.translate(mpObj->GetAnchorPos());
                }

                mpObj->setSdrObjectTransformation(aNewObjectMatrix);
			    return true;
		    }
		    break;
	    }

	    case OWN_ATTR_ZORDER:
	    {
		    sal_Int32 nNewOrdNum = 0;
		    if(rValue >>= nNewOrdNum)
		    {
                SdrObjList* pObjList = mpObj->getParentOfSdrObject();
			    if( pObjList )
			    {
#ifdef DBG_UTIL
				    SdrObject* pCheck =
#endif
				        pObjList->SetNavigationPosition( mpObj->GetNavigationPosition(), (sal_uInt32)nNewOrdNum );
				        DBG_ASSERT( pCheck == mpObj.get(), "GetNavigationPosition() failed!" );
			    }
			    return true;
		    }
		    break;
	    }
	    case OWN_ATTR_FRAMERECT:
	    {
		    awt::Rectangle aUnoRect;
		    if(rValue >>= aUnoRect)
		    {
			    basegfx::B2DPoint aTopLeft( aUnoRect.X, aUnoRect.Y );
			    basegfx::B2DPoint aObjSize( aUnoRect.Width, aUnoRect.Height );
			    
			    ForceMetricToItemPoolMetric(aTopLeft);
			    ForceMetricToItemPoolMetric(aObjSize);
			    
			    basegfx::B2DRange aRange(aTopLeft, aTopLeft + aObjSize);

			    sdr::legacy::SetSnapRange(*mpObj.get(), aRange);
			    return true;
		    }
		    break;
	    }
	    case OWN_ATTR_MIRRORED:
	    {
		    sal_Bool bMirror = sal_Bool();
		    if(rValue >>= bMirror )
		    {
			    SdrGrafObj* pObj = dynamic_cast< SdrGrafObj* >( mpObj.get() );
			    if( pObj )
				    pObj->SetMirrored(bMirror);
			    return true;
		    }
		    break;
	    }
	    case OWN_ATTR_EDGE_START_OBJ:
	    case OWN_ATTR_EDGE_END_OBJ:
	    case OWN_ATTR_GLUEID_HEAD:
	    case OWN_ATTR_GLUEID_TAIL:
	    case OWN_ATTR_EDGE_START_POS:
	    case OWN_ATTR_EDGE_END_POS:
	    case OWN_ATTR_EDGE_POLYPOLYGONBEZIER:
	    {
		    SdrEdgeObj* pEdgeObj = dynamic_cast< SdrEdgeObj* >(mpObj.get());
		    if(pEdgeObj)
		    {
			    switch(pProperty->nWID)
			    {
			        case OWN_ATTR_EDGE_START_OBJ:
			        case OWN_ATTR_EDGE_END_OBJ:
				    {
					    Reference< drawing::XShape > xShape;
					    if( rValue >>= xShape )
					    {
						    SdrObject* pNode = GetSdrObjectFromXShape( xShape );
						    if( pNode )
						    {
							    pEdgeObj->ConnectToNode( pProperty->nWID == OWN_ATTR_EDGE_START_OBJ, pNode );
							    pEdgeObj->setGluePointIndex( pProperty->nWID == OWN_ATTR_EDGE_START_OBJ, -1 );
							    return true;
						    }
					    }
					    break;
				    }

			        case OWN_ATTR_EDGE_START_POS:
			        case OWN_ATTR_EDGE_END_POS:
				    {
					    awt::Point aUnoPoint;
					    if( rValue >>= aUnoPoint )
					    {
						        basegfx::B2DPoint aPoint( aUnoPoint.X, aUnoPoint.Y );

                            ForceMetricToItemPoolMetric( aPoint );
    			            
                                if(isWriterAnchorUsed())
                                {
                                aPoint += mpObj->GetAnchorPos();
                                }

						    pEdgeObj->SetTailPoint( pProperty->nWID == OWN_ATTR_EDGE_START_POS, aPoint );
						    return true;
					    }
					    break;
				    }

			        case OWN_ATTR_GLUEID_HEAD:
			        case OWN_ATTR_GLUEID_TAIL:
				    {
					    sal_Int32 nId = 0;
					    if( rValue >>= nId )
					    {
						    pEdgeObj->setGluePointIndex( pProperty->nWID == OWN_ATTR_GLUEID_HEAD, nId );
						    return true;
					    }
					    break;
				    }
    			    case OWN_ATTR_EDGE_POLYPOLYGONBEZIER:
				    {
					    drawing::PolyPolygonBezierCoords aPolyPoly;
					    if ( rValue >>= aPolyPoly )
					    {
						        basegfx::B2DPolyPolygon aNewPolyPolygon(basegfx::tools::UnoPolyPolygonBezierCoordsToB2DPolyPolygon(aPolyPoly));
                            ForceMetricToItemPoolMetric( aNewPolyPolygon );
    			            
                                if(isWriterAnchorUsed())
                            {
							        aNewPolyPolygon.transform(basegfx::tools::createTranslateB2DHomMatrix(mpObj->GetAnchorPos()));
                            }
						    
                            pEdgeObj->SetEdgeTrackPath( aNewPolyPolygon );
                            return true;
					    }
				    }
			    }
		    }
		    break;
	    }
	    case OWN_ATTR_MEASURE_START_POS:
	    case OWN_ATTR_MEASURE_END_POS:
	    {
		    SdrMeasureObj* pMeasureObj = dynamic_cast< SdrMeasureObj* >(mpObj.get());
		    awt::Point aUnoPoint;

		    if(pMeasureObj && ( rValue >>= aUnoPoint ) )
		    {
                    const SdrObjectChangeBroadcaster aSdrObjectChangeBroadcaster(*pMeasureObj);
			        basegfx::B2DPoint aPoint( aUnoPoint.X, aUnoPoint.Y );
                ForceMetricToItemPoolMetric( aPoint );

                    if(isWriterAnchorUsed())
                    {
				    aPoint += mpObj->GetAnchorPos();
                    }

			        pMeasureObj->SetObjectPoint( aPoint, pProperty->nWID == OWN_ATTR_MEASURE_START_POS ? 0L : 1L );
			    pMeasureObj->SetChanged();
			    return true;
		    }
		    break;
	    }
	    case OWN_ATTR_FILLBMP_MODE:
	    {
		    drawing::BitmapMode eMode;
		    if(!(rValue >>= eMode) )
		    {
			    sal_Int32 nMode = 0;
			    if(!(rValue >>= nMode))
				    break;

			    eMode = (drawing::BitmapMode)nMode;
		    }
		    mpObj->SetMergedItem( XFillBmpStretchItem( eMode == drawing::BitmapMode_STRETCH ) );
		    mpObj->SetMergedItem( XFillBmpTileItem( eMode == drawing::BitmapMode_REPEAT ) );
		    return true;
	    }

	    case SDRATTR_LAYERID:
	    {
		    sal_Int16 nLayerId = sal_Int16();
		    if( rValue >>= nLayerId )
		    {
			        SdrLayer* pLayer = mpModel->GetModelLayerAdmin().GetLayerPerID((unsigned char)nLayerId);
			    if( pLayer )
			    {
				    mpObj->SetLayer((unsigned char)nLayerId);
				    return true;
			    }
		    }
		    break;
	    }

	    case SDRATTR_LAYERNAME:
	    {
		    OUString aLayerName;
		    if( rValue >>= aLayerName )
		    {
		        const SdrLayer* pLayer=mpModel->GetModelLayerAdmin().GetLayer(aLayerName, true);
			    if( pLayer != NULL )
			    {
				    mpObj->SetLayer( pLayer->GetID() );
				    return true;
			    }
		    }
		    break;
	    }
	    case SDRATTR_ROTATEANGLE:
	    {
		    sal_Int32 nAngle = 0;
		    if( rValue >>= nAngle )
		    {
			    const long aOldRotation(sdr::legacy::GetRotateAngle(*mpObj.get()));
			    nAngle -= aOldRotation;
			    if (nAngle!=0)
			    {
				        const Point aRef1(sdr::legacy::GetSnapRect(*mpObj.get()).Center());
				        sdr::legacy::RotateSdrObject(*mpObj.get(), aRef1, nAngle);
			    }
			    return true;
		    }

		    break;
	    }

	    case SDRATTR_SHEARANGLE:
	    {
		    sal_Int32 nShear = 0;
		    if( rValue >>= nShear )
		    {
			    const long aOldShear(sdr::legacy::GetShearAngleX(*mpObj.get()));
			    nShear -= aOldShear;
			    if(nShear != 0 )
			    {
				        Point aRef1(sdr::legacy::GetSnapRect(*mpObj.get()).Center());
				        sdr::legacy::ShearSdrObject(*mpObj.get(), aRef1, nShear, false);
				    return true;
			    }
		    }

		    break;
	    }

	    case SDRATTR_OBJMOVEPROTECT:
	    {
		    sal_Bool bMoveProtect = sal_Bool();
		    if( rValue >>= bMoveProtect )
		    {
			    mpObj->SetMoveProtect(bMoveProtect);
			    return true;
		    }
		    break;
	    }
	    case SDRATTR_OBJECTNAME:
	    {
		    OUString aName;
		    if( rValue >>= aName )
		    {
			    mpObj->SetName( aName );
			    return true;
		    }
		    break;
	    }

	    // #i68101#
	    case OWN_ATTR_MISC_OBJ_TITLE:
	    {
		    OUString aTitle;
		    if( rValue >>= aTitle )
		    {
			    mpObj->SetTitle( aTitle );
			    return true;
		    }
		    break;
	    }
	    case OWN_ATTR_MISC_OBJ_DESCRIPTION:
	    {
		    OUString aDescription;
		    if( rValue >>= aDescription )
		    {
			    mpObj->SetDescription( aDescription );
			    return true;
		    }
		    break;
	    }

	    case SDRATTR_OBJPRINTABLE:
	    {
		    sal_Bool bPrintable = sal_Bool();
		    if( rValue >>= bPrintable )
		    {
			    mpObj->SetPrintable(bPrintable);
			    return true;
		    }
		    break;
	    }
	    case SDRATTR_OBJVISIBLE:
	    {
		    sal_Bool bVisible = sal_Bool();
		    if( rValue >>= bVisible )
		    {
			    mpObj->SetVisible(bVisible);
			    return true;
		    }
		    break;
	    }
	    case SDRATTR_OBJSIZEPROTECT:
	    {
		    sal_Bool bResizeProtect = sal_Bool();
		    if( rValue >>= bResizeProtect )
		    {
			    mpObj->SetResizeProtect(bResizeProtect);
			    return true;
		    }
		    break;
	    }
	    case OWN_ATTR_PAGE_NUMBER:
	    {
		    sal_Int32 nPageNum = 0;
		    if( (rValue >>= nPageNum) && ( nPageNum >= 0 ) && ( nPageNum <= 0xffff ) )
		    {
			    SdrPageObj* pPageObj = dynamic_cast< SdrPageObj* >(mpObj.get());
			    if( pPageObj )
			    {
			        SdrPage* pNewPage = 0;
				    const sal_uInt16 nDestinationPageNum((sal_uInt16)((nPageNum << 1L) - 1L));

				    if(nDestinationPageNum < pPageObj->getSdrModelFromSdrObject().GetPageCount())
				    {
					    pNewPage = pPageObj->getSdrModelFromSdrObject().GetPage(nDestinationPageNum);
				    }

				    pPageObj->SetReferencedPage(pNewPage);
			    }

			    return true;
		    }
		    break;
	    }
	    case XATTR_FILLBITMAP:
	    case XATTR_FILLGRADIENT:
	    case XATTR_FILLHATCH:
	    case XATTR_FILLFLOATTRANSPARENCE:
	    case XATTR_LINEEND:
	    case XATTR_LINESTART:
	    case XATTR_LINEDASH:
	    {
		    if( pProperty->nMemberId == MID_NAME )
		    {
			    OUString aApiName;
			    if( rValue >>= aApiName )
			    {
				    if( SetFillAttribute( pProperty->nWID, aApiName ) )
					    return true;
			    }
			    break;
		    }
		    else
		    {
			    return false;
		    }
	    }
		case OWN_ATTR_CIRCSTARTANGLE:
		case OWN_ATTR_CIRCENDANGLE:
		{
		    sal_Int32 nOldAngle(0);
		    
			if(rValue >>= nOldAngle)
			{
				SdrCircObj* pSdrCircObj = dynamic_cast< SdrCircObj* >(mpObj.get());
				
				if(pSdrCircObj)
				{
					const double fAngle(((36000 - (nOldAngle % 36000)) * F_PI) / 18000.0);

					if(OWN_ATTR_CIRCSTARTANGLE == pProperty->nWID)
					{
						pSdrCircObj->SetStartAngle(fAngle);
					}
					else
					{
						pSdrCircObj->SetEndAngle(fAngle);
					}

					return true;
				}
			}

			break;
		}
		case OWN_ATTR_CIRCKIND:
		{
			drawing::CircleKind eKind(drawing::CircleKind_FULL);

			if(rValue >>= eKind)
			{
				SdrCircObj* pSdrCircObj = dynamic_cast< SdrCircObj* >(mpObj.get());
				
				if(pSdrCircObj)
				{
					SdrObjKind eNewKind(OBJ_CIRC);

					switch(eKind)
					{
						default : // case drawing::CircleKind_FULL:
						{
							eNewKind = OBJ_CIRC;
							break;
						}
						case drawing::CircleKind_CUT:
						{
							eNewKind = OBJ_CCUT;
							break;
						}
						case drawing::CircleKind_ARC:
						{
							eNewKind = OBJ_CARC;
							break;
						}
						case drawing::CircleKind_SECTION:
						{
							eNewKind = OBJ_SECT;
							break;
						}
					}
					
					pSdrCircObj->SetCircleKind(eNewKind);
					return true;
				}
			}

			break;
		}
	    default:
	    {
		    return false;
	    }
	}
	throw lang::IllegalArgumentException();
}

//----------------------------------------------------------------------

bool SvxShape::getPropertyValueImpl( const ::rtl::OUString&, const SfxItemPropertySimpleEntry* pProperty, ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException)
{
	switch( pProperty->nWID )
	{
/*
    case OWN_ATTR_HASLEVELS:
	{
		rValue <<= SvxTextEditSource::hasLevels( mpObj.get() );
		break;
	}
*/
	    case OWN_ATTR_CAPTION_POINT:
	    {
		    awt::Point aPnt(0, 0);

		    if(mpObj.is())
    		{
                // get base geometry
                basegfx::B2DPoint aVclPoint(((SdrCaptionObj*)mpObj.get())->GetTailPos());

		        // #90763# pos is absolute, make it relative to top left
				aVclPoint -= mpObj->getSdrObjectTranslate();
//		        const basegfx::B2DHomMatrix aObjectMatrix(mpObj->getSdrObjectTransformation());
//		        aVclPoint.X() -= basegfx::fround(aObjectMatrix.get(0, 2));
//		        aVclPoint.Y() -= basegfx::fround(aObjectMatrix.get(1, 2));

		        // #88491# make pos relative to anchor
				// Not needed anymore, pos is already relative to object's
				// TopLeft, anchor is not relevant for relative position
	            //if(isWriterAnchorUsed())
		        //{
			    //    aVclPoint -= sdr::legacy::GetAnchorPos(*mpObj.get());
		        //}

		        // #88657# metric of pool maybe twips (writer)
		        ForceMetricTo100th_mm(aVclPoint);

                // convert to needed data format
                aPnt.X = basegfx::fround(aVclPoint.getX());
                aPnt.Y = basegfx::fround(aVclPoint.getY());
            }

		    rValue <<= aPnt;
		    break;
	    }

	    case OWN_ATTR_TRANSFORMATION:
	    {
		    drawing::HomogenMatrix3 aMatrix;

		    if(mpObj.is())
		    {
                // get base geometry
		        basegfx::B2DHomMatrix aObjectMatrix(mpObj->getSdrObjectTransformation());

		        // make pos relative to anchor
	            if(isWriterAnchorUsed())
		        {
                    aObjectMatrix.translate(-mpObj->GetAnchorPos());
		        }

                // #88657# metric of pool maybe twips (writer)
		        ForceMetricTo100th_mm(aObjectMatrix);

                // convert to needed data format
                basegfx::tools::B2DHomMatrixToUnoHomogenMatrix3(aObjectMatrix, aMatrix);
		    }

		    rValue <<= aMatrix;
		    break;
	    }

	    case OWN_ATTR_ZORDER:
	    {
	        rValue <<= (sal_Int32)mpObj->GetNavigationPosition();
		    break;
	    }

	    case OWN_ATTR_BITMAP:
	    {
		    rValue = GetBitmap();
		    if(!rValue.hasValue())
			    throw uno::RuntimeException();

		    break;
	    }

	    case OWN_ATTR_ISFONTWORK:
	    {
			SdrTextObj* pSdrTextObj = dynamic_cast< SdrTextObj* >(mpObj.get());
			rValue <<= pSdrTextObj ? (sal_Bool)pSdrTextObj->IsFontwork() : sal_False;
		    break;
	    }

	    case OWN_ATTR_FRAMERECT:
	    {
		    const basegfx::B2DRange aRange( sdr::legacy::GetSnapRange(*mpObj.get()) );
		    basegfx::B2DPoint aTopLeft( aRange.getMinimum() );
		    basegfx::B2DPoint aObjSize( aRange.getRange() );
		    
		    ForceMetricTo100th_mm(aTopLeft);
		    ForceMetricTo100th_mm(aObjSize);
		    
		    ::com::sun::star::awt::Rectangle aUnoRect(
			        basegfx::fround(aTopLeft.getX()), 
				    basegfx::fround(aTopLeft.getY()),
			        basegfx::fround(aObjSize.getX()), 
				    basegfx::fround(aObjSize.getY()) );

		    rValue <<= aUnoRect;
		    break;
	    }

	    case OWN_ATTR_BOUNDRECT:
	    {
		    const basegfx::B2DRange aRange( mpObj->getObjectRange(0) );
		    basegfx::B2DPoint aTopLeft( aRange.getMinimum() );
		    basegfx::B2DPoint aObjSize( aRange.getRange() );

		    ForceMetricTo100th_mm(aTopLeft);
		    ForceMetricTo100th_mm(aObjSize);
		    
		    ::com::sun::star::awt::Rectangle aUnoRect(
			        basegfx::fround(aTopLeft.getX()), 
				    basegfx::fround(aTopLeft.getY()),
			        basegfx::fround(aObjSize.getX()), 
				    basegfx::fround(aObjSize.getY()) );

		    rValue <<= aUnoRect;
		    break;
	    }

	    case OWN_ATTR_LDNAME:
	    {
		    OUString aName( mpObj->GetName() );
		    rValue <<= aName;
		    break;
	    }

	    case OWN_ATTR_LDBITMAP:
	    {
		    sal_uInt16 nId;
		    if( mpObj->GetObjInventor() == SdrInventor && mpObj->GetObjIdentifier() == OBJ_OLE2 )
		    {
			    nId = RID_UNODRAW_OLE2;
		    }
		    else if( mpObj->GetObjInventor() == SdrInventor && mpObj->GetObjIdentifier() == OBJ_GRAF )
		    {
			    nId = RID_UNODRAW_GRAPHICS;
		    }
		    else
		    {
			    nId = RID_UNODRAW_OBJECTS;
		    }

		    BitmapEx aBmp( SVX_RES(nId) );
		    Reference< awt::XBitmap > xBmp( VCLUnoHelper::CreateBitmap( aBmp ) );

		    rValue <<= xBmp;
		    break;
	    }

	    case OWN_ATTR_MIRRORED:
	    {
			SdrGrafObj* pSdrGrafObj = dynamic_cast< SdrGrafObj* >(mpObj.get());
		    rValue <<= pSdrGrafObj ? (sal_Bool)pSdrGrafObj->IsMirrored() : sal_False;
	    }

	    case OWN_ATTR_EDGE_START_OBJ:
	    case OWN_ATTR_EDGE_START_POS:
	    case OWN_ATTR_EDGE_END_POS:
	    case OWN_ATTR_EDGE_END_OBJ:
	    case OWN_ATTR_GLUEID_HEAD:
	    case OWN_ATTR_GLUEID_TAIL:
	    case OWN_ATTR_EDGE_POLYPOLYGONBEZIER:
	    {
		    SdrEdgeObj* pEdgeObj = dynamic_cast<SdrEdgeObj*>(mpObj.get());
		    if(pEdgeObj)
		    {
			    switch(pProperty->nWID)
			    {
			        case OWN_ATTR_EDGE_START_OBJ:
			        case OWN_ATTR_EDGE_END_OBJ:
				    {
					    SdrObject* pNode = pEdgeObj->GetConnectedNode(pProperty->nWID == OWN_ATTR_EDGE_START_OBJ);
					    if(pNode)
					    {
						    Reference< drawing::XShape > xShape( GetXShapeForSdrObject( pNode ) );
						    if(xShape.is())
							    rValue <<= xShape;

					    }
					    break;
				    }

			        case OWN_ATTR_EDGE_START_POS:
			        case OWN_ATTR_EDGE_END_POS:
				    {
						basegfx::B2DPoint aPoint(pEdgeObj->GetTailPoint( pProperty->nWID == OWN_ATTR_EDGE_START_POS ));
   			            
                        if(isWriterAnchorUsed())
                        {
						    aPoint -= mpObj->GetAnchorPos();
                        }

					    ForceMetricTo100th_mm( aPoint );
					    
                        awt::Point aUnoPoint( basegfx::fround(aPoint.getX()), basegfx::fround(aPoint.getY()) );

					    rValue <<= aUnoPoint;
					    break;
				    }
			        case OWN_ATTR_GLUEID_HEAD:
			        case OWN_ATTR_GLUEID_TAIL:
				    {
					    rValue <<= pEdgeObj->getGluePointIndex( pProperty->nWID == OWN_ATTR_GLUEID_HEAD );
					    break;
				    }
    			    case OWN_ATTR_EDGE_POLYPOLYGONBEZIER:
				    {
					    basegfx::B2DPolyPolygon aPolyPoly( pEdgeObj->GetEdgeTrackPath() );
  			            
                        if(isWriterAnchorUsed())
					    {
	            	        aPolyPoly.transform(basegfx::tools::createTranslateB2DHomMatrix(-mpObj->GetAnchorPos()));
					    }

                        ForceMetricTo100th_mm( aPolyPoly );
                        
					    drawing::PolyPolygonBezierCoords aRetval;
					        basegfx::tools::B2DPolyPolygonToUnoPolyPolygonBezierCoords(aPolyPoly, aRetval);
					    rValue <<= aRetval;
					    break;
				    }
			    }
		    }
		    break;
	    }

	    case OWN_ATTR_MEASURE_START_POS:
	    case OWN_ATTR_MEASURE_END_POS:
	    {
		    SdrMeasureObj* pMeasureObj = dynamic_cast<SdrMeasureObj*>(mpObj.get());
		    if(pMeasureObj)
		    {
			    basegfx::B2DPoint aPoint( pMeasureObj->GetObjectPoint( pProperty->nWID == OWN_ATTR_MEASURE_START_POS ? 0 : 1 ) );
                
                if(isWriterAnchorUsed())
                {
				    aPoint -= mpObj->GetAnchorPos();
                }

                ForceMetricTo100th_mm( aPoint );

                awt::Point aUnoPoint( basegfx::fround(aPoint.getX()), basegfx::fround(aPoint.getY()) );

			    rValue <<= aUnoPoint;
			    break;
		    }
		    break;
	    }

	    case OWN_ATTR_FILLBMP_MODE:
	    {
		    const SfxItemSet& rObjItemSet = mpObj->GetMergedItemSet();

		    XFillBmpStretchItem* pStretchItem = (XFillBmpStretchItem*)&rObjItemSet.Get(XATTR_FILLBMP_STRETCH);
		    XFillBmpTileItem* pTileItem = (XFillBmpTileItem*)&rObjItemSet.Get(XATTR_FILLBMP_TILE);

		    if( pTileItem && pTileItem->GetValue() )
		    {
			    rValue <<= drawing::BitmapMode_REPEAT;
		    }
		    else if( pStretchItem && pStretchItem->GetValue() )
		    {
			    rValue <<= drawing::BitmapMode_STRETCH;
		    }
		    else
		    {
			    rValue <<= drawing::BitmapMode_NO_REPEAT;
		    }
		    break;
	    }
	    case SDRATTR_LAYERID:
		    rValue <<= (sal_Int16)mpObj->GetLayer();
		    break;

	    case SDRATTR_LAYERNAME:
	    {
	        SdrLayer* pLayer = mpModel->GetModelLayerAdmin().GetLayerPerID(mpObj->GetLayer());
		    if( pLayer )
		    {
			    OUString aName( pLayer->GetName() );
			    rValue <<= aName;
		    }
		    break;
	    }

	    case SDRATTR_ROTATEANGLE:
		{
			const long aOldRotation(sdr::legacy::GetRotateAngle(*mpObj.get()));
		    rValue <<= aOldRotation;
		    break;
	    }

	    case SDRATTR_SHEARANGLE:
		{
			const long aOldShear(sdr::legacy::GetShearAngleX(*mpObj.get()));
		    rValue <<= aOldShear;
		    break;
	    }

	    case SDRATTR_OBJMOVEPROTECT:
		    rValue = uno::makeAny( (sal_Bool) mpObj->IsMoveProtect() );
		    break;

	    case SDRATTR_OBJECTNAME:
	    {
		    OUString aName( mpObj->GetName() );
		    rValue <<= aName;
		    break;
	    }

	    // #i68101#
	    case OWN_ATTR_MISC_OBJ_TITLE:
	    {
		    OUString aTitle( mpObj->GetTitle() );
		    rValue <<= aTitle;
		    break;
	    }

	    case OWN_ATTR_MISC_OBJ_DESCRIPTION:
	    {
		    OUString aDescription( mpObj->GetDescription() );
		    rValue <<= aDescription;
		    break;
	    }

	    case SDRATTR_OBJPRINTABLE:
	        rValue = uno::makeAny( (sal_Bool) mpObj->IsPrintable() );
		    break;

	    case SDRATTR_OBJVISIBLE:
		    rValue <<= static_cast<sal_Bool>( mpObj->IsVisible() );
		    break;

	    case SDRATTR_OBJSIZEPROTECT:
	        rValue = uno::makeAny( (sal_Bool)mpObj->IsResizeProtect() );
		    break;

	    case OWN_ATTR_PAGE_NUMBER:
	    {
		    SdrPageObj* pPageObj = dynamic_cast<SdrPageObj*>(mpObj.get());
		    if(pPageObj)
		    {
			    SdrPage* pPage = pPageObj->GetReferencedPage();
			        sal_Int32 nPageNumber = (pPage) ? pPage->GetPageNumber() : 0;
			    nPageNumber++;
			    nPageNumber >>= 1;
			    rValue <<= nPageNumber;
		    }
		    break;
	    }

	    case OWN_ATTR_UINAME_SINGULAR:
	    {
		    String aTmp;
		    mpObj->TakeObjNameSingul( aTmp );
		    rValue <<= OUString( aTmp );
		    break;
	    }

	    case OWN_ATTR_UINAME_PLURAL:
	    {
		    String aTmp;
		    mpObj->TakeObjNamePlural( aTmp );
		    rValue <<= OUString( aTmp );
		    break;
	    }
	    case OWN_ATTR_METAFILE:
	    {
		    SdrOle2Obj* pObj = dynamic_cast<SdrOle2Obj*>(mpObj.get());
		    if( pObj )
		    {
                Graphic* pGraphic = pObj->GetGraphic();
                if( pGraphic )
			    {
                    bool bIsWMF = false;
                    if ( pGraphic->IsLink() )
                    {
                        GfxLink aLnk = pGraphic->GetLink();
                        if ( aLnk.GetType() == GFX_LINK_TYPE_NATIVE_WMF )
                        {
                                bIsWMF = true;
                            uno::Sequence<sal_Int8> aSeq((sal_Int8*)aLnk.GetData(), (sal_Int32) aLnk.GetDataSize());
                            rValue <<= aSeq;
                        }
                    }
                    if ( !bIsWMF )
                    {
					    GDIMetaFile aMtf;
					    if ( pGraphic->GetType() != GRAPHIC_BITMAP )
						    aMtf = pObj->GetGraphic()->GetGDIMetaFile();
					    else
					    {
						    VirtualDevice aVirDev;
						    aMtf.Record( &aVirDev );
						    pGraphic->Draw( &aVirDev, Point(),  pGraphic->GetPrefSize() );
						    aMtf.Stop();
						    aMtf.SetPrefSize( pGraphic->GetPrefSize() );
						    aMtf.SetPrefMapMode( pGraphic->GetPrefMapMode() );
					    }
                        SvMemoryStream aDestStrm( 65535, 65535 );
                        ConvertGDIMetaFileToWMF( aMtf, aDestStrm, NULL, sal_False );
                        const uno::Sequence<sal_Int8> aSeq(
                            static_cast< const sal_Int8* >(aDestStrm.GetData()),
                            aDestStrm.GetEndOfData());
                        rValue <<= aSeq;
                    }
			    }
		    }
		    else
		    {
			    rValue = GetBitmap( sal_True );
		    }
		    break;
	    }

		case OWN_ATTR_CIRCSTARTANGLE:
		case OWN_ATTR_CIRCENDANGLE:
		{
			SdrCircObj* pSdrCircObj = dynamic_cast< SdrCircObj* >(mpObj.get());

			if(pSdrCircObj && OBJ_CIRC != mpObj->GetObjIdentifier())
			{
				const double fAngle(OWN_ATTR_CIRCSTARTANGLE == pProperty->nWID ? pSdrCircObj->GetStartAngle() : pSdrCircObj->GetEndAngle());
				const sal_Int32 nOldAngle(basegfx::fround(((F_2PI - fAngle) * 18000.0) / F_PI) % 36000);

				rValue <<= nOldAngle;
			}

			break;
		}
		case OWN_ATTR_CIRCKIND:
		{
			if(SdrInventor == mpObj->GetObjInventor())
			{
				drawing::CircleKind eKind;

				switch(mpObj->GetObjIdentifier())
				{
				case OBJ_CIRC:			// Kreis, Ellipse
					eKind = drawing::CircleKind_FULL;
					break;
				case OBJ_CCUT:			// Kreisabschnitt
					eKind = drawing::CircleKind_CUT;
					break;
				case OBJ_CARC:			// Kreisbogen
					eKind = drawing::CircleKind_ARC;
					break;
				case OBJ_SECT:			// Kreissektor
					eKind = drawing::CircleKind_SECTION;
					break;
				}

				rValue <<= eKind;
			}
			break;
		}

	    default:
        {
		    return false;
	    }
	}
	return true;
}

//----------------------------------------------------------------------

bool SvxShape::getPropertyStateImpl( const SfxItemPropertySimpleEntry* pProperty, ::com::sun::star::beans::PropertyState& rState ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::uno::RuntimeException)
{
	if( pProperty->nWID == OWN_ATTR_FILLBMP_MODE )
	{
		const SfxItemSet& rSet = mpObj->GetMergedItemSet();

		if( rSet.GetItemState( XATTR_FILLBMP_STRETCH, false ) == SFX_ITEM_SET ||
			rSet.GetItemState( XATTR_FILLBMP_TILE, false ) == SFX_ITEM_SET )
		{
			rState = beans::PropertyState_DIRECT_VALUE;
		}
		else
		{
			rState = beans::PropertyState_AMBIGUOUS_VALUE;
		}
	}
	else if((( pProperty->nWID >= OWN_ATTR_VALUE_START && pProperty->nWID <= OWN_ATTR_VALUE_END ) ||
	   ( pProperty->nWID >= SDRATTR_NOTPERSIST_FIRST && pProperty->nWID <= SDRATTR_NOTPERSIST_LAST )) && ( pProperty->nWID != SDRATTR_TEXTDIRECTION ) )
	{
		rState = beans::PropertyState_DIRECT_VALUE;
	}
	else
	{
		return false;
	}

	return true;
}

//----------------------------------------------------------------------

bool SvxShape::setPropertyToDefaultImpl( const SfxItemPropertySimpleEntry* pProperty ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::uno::RuntimeException)
{
	if( pProperty->nWID == OWN_ATTR_FILLBMP_MODE )
	{
		mpObj->ClearMergedItem( XATTR_FILLBMP_STRETCH );
		mpObj->ClearMergedItem( XATTR_FILLBMP_TILE );
		return true;
	}
	else if((pProperty->nWID >= OWN_ATTR_VALUE_START && pProperty->nWID <= OWN_ATTR_VALUE_END ) ||
	   ( pProperty->nWID >= SDRATTR_NOTPERSIST_FIRST && pProperty->nWID <= SDRATTR_NOTPERSIST_LAST ))
	{
		return true;
	}
	else
	{
		return false;
	}
}

//----------------------------------------------------------------------

uno::Sequence< beans::PropertyState > SAL_CALL SvxShape::getPropertyStates( const uno::Sequence< OUString >& aPropertyName )
	throw(beans::UnknownPropertyException, uno::RuntimeException)
{
	const sal_Int32 nCount = aPropertyName.getLength();
	const OUString* pNames = aPropertyName.getConstArray();

	uno::Sequence< beans::PropertyState > aRet( nCount );
	beans::PropertyState* pState = aRet.getArray();

	if( mpImpl->mpMaster )
	{
		for( sal_Int32 nIdx = 0; nIdx < nCount; nIdx++ )
			pState[nIdx] = getPropertyState( pNames[nIdx] );
	}
	else
	{
		for( sal_Int32 nIdx = 0; nIdx < nCount; nIdx++ )
			pState[nIdx] = getPropertyState( pNames[nIdx] );
	}

	return aRet;
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::setPropertyToDefault( const OUString& PropertyName )
	throw(beans::UnknownPropertyException, uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		mpImpl->mpMaster->setPropertyToDefault( PropertyName );
	}
	else
	{
		_setPropertyToDefault( PropertyName );
	}
}

void SAL_CALL SvxShape::_setPropertyToDefault( const OUString& PropertyName )
	throw(beans::UnknownPropertyException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

    const SfxItemPropertySimpleEntry* pProperty = mpPropSet->getPropertyMapEntry(PropertyName);

	if( !mpObj.is() || mpModel == NULL || pProperty == NULL )
		throw beans::UnknownPropertyException();

	if( !setPropertyToDefaultImpl( pProperty ) )
	{
		mpObj->ClearMergedItem( pProperty->nWID );
	}

	mpModel->SetChanged();
}

//----------------------------------------------------------------------

uno::Any SAL_CALL SvxShape::getPropertyDefault( const OUString& aPropertyName )
	throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		return mpImpl->mpMaster->getPropertyDefault( aPropertyName );
	}
	else
	{
		return _getPropertyDefault( aPropertyName );
	}
}

uno::Any SAL_CALL SvxShape::_getPropertyDefault( const OUString& aPropertyName )
	throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

    const SfxItemPropertySimpleEntry* pMap = mpPropSet->getPropertyMapEntry(aPropertyName);

	if( !mpObj.is() || pMap == NULL || mpModel == NULL )
		throw beans::UnknownPropertyException();

	if(( pMap->nWID >= OWN_ATTR_VALUE_START && pMap->nWID <= OWN_ATTR_VALUE_END ) ||
	   ( pMap->nWID >= SDRATTR_NOTPERSIST_FIRST && pMap->nWID <= SDRATTR_NOTPERSIST_LAST ))
	{
		return getPropertyValue( aPropertyName );
	}

	// Default aus ItemPool holen
	if(!mpModel->GetItemPool().IsWhich(pMap->nWID))
		throw beans::UnknownPropertyException();

	SfxItemSet aSet( mpModel->GetItemPool(),	pMap->nWID, pMap->nWID);
	aSet.Put(mpModel->GetItemPool().GetDefaultItem(pMap->nWID));

	return GetAnyForItem( aSet, pMap );
}

// XMultiPropertyStates
void SvxShape::setAllPropertiesToDefault() throw (uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( !mpObj.is() )
		throw lang::DisposedException();
	mpObj->ClearMergedItem(); // nWhich == 0 => all

    if(dynamic_cast< SdrGrafObj* >(mpObj.get()))
	{
        // defaults for graphic objects have changed:
        mpObj->SetMergedItem( XFillStyleItem( XFILL_NONE ) );
        mpObj->SetMergedItem( XLineStyleItem( XLINE_NONE ) );
	}

	// #i68523# special handling for Svx3DCharacterModeItem, this is not saved
	// but needs to be sal_True in svx, pool default (false) in sch. Since sch
	// does not load lathe or extrude objects, it is possible to set the items
	// here.
	// For other solution possibilities, see task description.
    if(dynamic_cast< E3dLatheObj* >(mpObj.get()) || dynamic_cast< E3dExtrudeObj* >(mpObj.get()))
	{
        mpObj->SetMergedItem(Svx3DCharacterModeItem(true));
	}

	mpModel->SetChanged();
}

void SvxShape::setPropertiesToDefault(
    const uno::Sequence<OUString>& aPropertyNames )
    throw (beans::UnknownPropertyException, uno::RuntimeException)
{
    for ( sal_Int32 pos = 0; pos < aPropertyNames.getLength(); ++pos )
        setPropertyToDefault( aPropertyNames[pos] );
}

uno::Sequence<uno::Any> SvxShape::getPropertyDefaults(
    const uno::Sequence<OUString>& aPropertyNames )
    throw (beans::UnknownPropertyException, lang::WrappedTargetException,
           uno::RuntimeException)
{
    ::std::vector<uno::Any> ret;
    for ( sal_Int32 pos = 0; pos < aPropertyNames.getLength(); ++pos )
        ret.push_back( getPropertyDefault( aPropertyNames[pos] ) );
    return uno::Sequence<uno::Any>( &ret[0], ret.size() );
}

//----------------------------------------------------------------------

//----------------------------------------------------------------------
// XServiceInfo
//----------------------------------------------------------------------
OUString SAL_CALL SvxShape::getImplementationName()
	throw(uno::RuntimeException)
{
	static OUString aServiceName( RTL_CONSTASCII_USTRINGPARAM("SvxShape") );
	return aServiceName;
}

#define STAR_NAMESPACE "com.sun.star."

const char* sUNO_service_style_ParagraphProperties			= STAR_NAMESPACE "style.ParagraphProperties";
const char* sUNO_service_style_ParagraphPropertiesComplex	= STAR_NAMESPACE "style.ParagraphPropertiesComplex";
const char* sUNO_service_style_ParagraphPropertiesAsian		= STAR_NAMESPACE "style.ParagraphPropertiesAsian";
const char* sUNO_service_style_CharacterProperties			= STAR_NAMESPACE "style.CharacterProperties";
const char* sUNO_service_style_CharacterPropertiesComplex	= STAR_NAMESPACE "style.CharacterPropertiesComplex";
const char* sUNO_service_style_CharacterPropertiesAsian		= STAR_NAMESPACE "style.CharacterPropertiesAsian";

const char* sUNO_service_drawing_FillProperties				= STAR_NAMESPACE "drawing.FillProperties";
const char* sUNO_service_drawing_TextProperties				= STAR_NAMESPACE "drawing.TextProperties";
const char* sUNO_service_drawing_LineProperties				= STAR_NAMESPACE "drawing.LineProperties";
const char* sUNO_service_drawing_ConnectorProperties		= STAR_NAMESPACE "drawing.ConnectorProperties";
const char* sUNO_service_drawing_MeasureProperties			= STAR_NAMESPACE "drawing.MeasureProperties";
const char* sUNO_service_drawing_ShadowProperties			= STAR_NAMESPACE "drawing.ShadowProperties";

const char* sUNO_service_drawing_RotationDescriptor			= STAR_NAMESPACE "drawing.RotationDescriptor";

const char* sUNO_service_drawing_Text						= STAR_NAMESPACE "drawing.Text";
const char* sUNO_service_drawing_GroupShape					= STAR_NAMESPACE "drawing.GroupShape";

const char* sUNO_service_drawing_CustomShapeProperties		= STAR_NAMESPACE "drawing.CustomShapeProperties";
const char* sUNO_service_drawing_CustomShape					= STAR_NAMESPACE "drawing.CustomShape";

const char* sUNO_service_drawing_PolyPolygonDescriptor		= STAR_NAMESPACE "drawing.PolyPolygonDescriptor";
const char* sUNO_service_drawing_PolyPolygonBezierDescriptor= STAR_NAMESPACE "drawing.PolyPolygonBezierDescriptor";

const char* sUNO_service_drawing_LineShape					= STAR_NAMESPACE "drawing.LineShape";
const char* sUNO_service_drawing_Shape						= STAR_NAMESPACE "drawing.Shape";
const char* sUNO_service_drawing_RectangleShape				= STAR_NAMESPACE "drawing.RectangleShape";
const char* sUNO_service_drawing_EllipseShape				= STAR_NAMESPACE "drawing.EllipseShape";
const char* sUNO_service_drawing_PolyPolygonShape			= STAR_NAMESPACE "drawing.PolyPolygonShape";
const char* sUNO_service_drawing_PolyLineShape				= STAR_NAMESPACE "drawing.PolyLineShape";
const char* sUNO_service_drawing_OpenBezierShape			= STAR_NAMESPACE "drawing.OpenBezierShape";
const char* sUNO_service_drawing_ClosedBezierShape			= STAR_NAMESPACE "drawing.ClosedBezierShape";
const char* sUNO_service_drawing_TextShape					= STAR_NAMESPACE "drawing.TextShape";
const char* sUNO_service_drawing_GraphicObjectShape			= STAR_NAMESPACE "drawing.GraphicObjectShape";
const char* sUNO_service_drawing_OLE2Shape					= STAR_NAMESPACE "drawing.OLE2Shape";
const char* sUNO_service_drawing_PageShape					= STAR_NAMESPACE "drawing.PageShape";
const char* sUNO_service_drawing_CaptionShape				= STAR_NAMESPACE "drawing.CaptionShape";
const char* sUNO_service_drawing_MeasureShape				= STAR_NAMESPACE "drawing.MeasureShape";
const char* sUNO_service_drawing_FrameShape					= STAR_NAMESPACE "drawing.FrameShape";
const char* sUNO_service_drawing_ControlShape				= STAR_NAMESPACE "drawing.ControlShape";
const char* sUNO_service_drawing_ConnectorShape				= STAR_NAMESPACE "drawing.ConnectorShape";
const char* sUNO_service_drawing_MediaShape					= STAR_NAMESPACE "drawing.MediaShape";


uno::Sequence< OUString > SAL_CALL SvxShape::getSupportedServiceNames()
	throw(uno::RuntimeException)
{
	if( mpImpl->mpMaster )
	{
		return mpImpl->mpMaster->getSupportedServiceNames();
	}
	else
	{
		return _getSupportedServiceNames();
	}
}

uno::Sequence< OUString > SAL_CALL SvxShape::_getSupportedServiceNames()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mpObj.is() && mpObj->GetObjInventor() == SdrInventor)
	{
		const sal_uInt16 nIdent = mpObj->GetObjIdentifier();

		switch(nIdent)
		{
    		case OBJ_GRUP:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_GroupServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_GroupServices, 2,
							sUNO_service_drawing_GroupShape,
					  		sUNO_service_drawing_Shape );

						pSeq = &SvxShape_GroupServices;
					}
				}

				return *pSeq;
			}
		    case OBJ_CUSTOMSHAPE:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_CustomShapeServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_CustomShapeServices, 13,
							sUNO_service_drawing_CustomShape,
					  		sUNO_service_drawing_Shape,
							sUNO_service_drawing_CustomShapeProperties,
							sUNO_service_drawing_FillProperties,
							sUNO_service_drawing_LineProperties,
							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,
							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);
						pSeq = &SvxShape_CustomShapeServices;
					}
				}
				return *pSeq;
			}
		    case OBJ_LINE:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_LineServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_LineServices,14,
							sUNO_service_drawing_LineShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_LineProperties,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_PolyPolygonDescriptor,
							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_LineServices;
					}
				}
				return *pSeq;
			}

		    case OBJ_RECT:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_RectServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_RectServices,14,
							sUNO_service_drawing_RectangleShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_FillProperties,
							sUNO_service_drawing_LineProperties,
							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);
						pSeq = &SvxShape_RectServices;
					}

				}
				return *pSeq;
			}

		    case OBJ_CIRC:
		    case OBJ_SECT:
		    case OBJ_CARC:
		    case OBJ_CCUT:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_CircServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_CircServices,14,
							sUNO_service_drawing_EllipseShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_FillProperties,
							sUNO_service_drawing_LineProperties,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_CircServices;
					}
				}

				return *pSeq;
			}

		    case OBJ_PATHPLIN:
		    case OBJ_PLIN:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_PathServices;
						comphelper::ServiceInfoHelper::addToSequence( SvxShape_PathServices,14,
							sUNO_service_drawing_PolyLineShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_LineProperties,

							sUNO_service_drawing_PolyPolygonDescriptor,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);
						pSeq = &SvxShape_PathServices;
					}
				}
				return *pSeq;
			}

		    case OBJ_PATHPOLY:
		    case OBJ_POLY:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_PolyServices;
						comphelper::ServiceInfoHelper::addToSequence( SvxShape_PolyServices,15,
							sUNO_service_drawing_PolyPolygonShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_LineProperties,
							sUNO_service_drawing_FillProperties,

							sUNO_service_drawing_PolyPolygonDescriptor,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_PolyServices;
					}
				}
				return *pSeq;
			}

		    case OBJ_FREELINE:
		    case OBJ_PATHLINE:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_FreeLineServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_FreeLineServices,15,
							sUNO_service_drawing_OpenBezierShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_LineProperties,
							sUNO_service_drawing_FillProperties,

							sUNO_service_drawing_PolyPolygonBezierDescriptor,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_FreeLineServices;
					}
				}

				return *pSeq;
			}

		    case OBJ_FREEFILL:
		    case OBJ_PATHFILL:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_FreeFillServices;
						comphelper::ServiceInfoHelper::addToSequence( SvxShape_FreeFillServices,15,
							sUNO_service_drawing_ClosedBezierShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_LineProperties,
							sUNO_service_drawing_FillProperties,

							sUNO_service_drawing_PolyPolygonBezierDescriptor,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_FreeFillServices;
					}
				}
				return *pSeq;
			}

		    case OBJ_OUTLINETEXT:
		    case OBJ_TITLETEXT:
		    case OBJ_TEXT:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_TextServices;
						comphelper::ServiceInfoHelper::addToSequence( SvxShape_TextServices,14,
							sUNO_service_drawing_TextShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_FillProperties,
							sUNO_service_drawing_LineProperties,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_TextServices;
					}
				}
				return *pSeq;
			}

    		case OBJ_GRAF:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_GrafServices;
						comphelper::ServiceInfoHelper::addToSequence( SvxShape_GrafServices, 12,
							sUNO_service_drawing_GraphicObjectShape,

							sUNO_service_drawing_Shape,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_GrafServices;
					}
				}
				return *pSeq;
			}

	    	case OBJ_OLE2:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_Ole2Services;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_Ole2Services, 2,
							sUNO_service_drawing_OLE2Shape,
							sUNO_service_drawing_Shape,

                            // #i118485# Added Text, Shadow and Rotation
							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_Ole2Services;
					}
				}
				return *pSeq;
			}

		    case OBJ_CAPTION:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_CaptionServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_CaptionServices,14,
							sUNO_service_drawing_CaptionShape,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_FillProperties,
							sUNO_service_drawing_LineProperties,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_CaptionServices;
					}
				}

				return *pSeq;
			}

		    case OBJ_PAGE:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_PageServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_PageServices, 2,
							sUNO_service_drawing_PageShape,
							sUNO_service_drawing_Shape );

						pSeq = &SvxShape_PageServices;
					}
				}

				return *pSeq;
			}

		    case OBJ_MEASURE:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_MeasureServices;
						comphelper::ServiceInfoHelper::addToSequence( SvxShape_MeasureServices,15,
							sUNO_service_drawing_MeasureShape,

							sUNO_service_drawing_MeasureProperties,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_LineProperties,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_PolyPolygonDescriptor,
							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_MeasureServices;
					}
				}

				return *pSeq;
			}

		    case OBJ_FRAME:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_FrameServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_FrameServices, 2,
							sUNO_service_drawing_FrameShape,
							sUNO_service_drawing_Shape );

						pSeq = &SvxShape_FrameServices;
					}
				}

				return *pSeq;
			}

		    case OBJ_UNO:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard _aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_UnoServices;
						comphelper::ServiceInfoHelper::addToSequence( SvxShape_UnoServices, 2,
							sUNO_service_drawing_ControlShape,
							sUNO_service_drawing_Shape );

						pSeq = &SvxShape_UnoServices;
					}
				}
				return *pSeq;
			}

		    case OBJ_EDGE:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_EdgeServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_EdgeServices,15,
							sUNO_service_drawing_ConnectorShape,
							sUNO_service_drawing_ConnectorProperties,

							sUNO_service_drawing_Shape,
							sUNO_service_drawing_LineProperties,

							sUNO_service_drawing_Text,
							sUNO_service_drawing_TextProperties,
							sUNO_service_style_ParagraphProperties,
							sUNO_service_style_ParagraphPropertiesComplex,
							sUNO_service_style_ParagraphPropertiesAsian,
							sUNO_service_style_CharacterProperties,
							sUNO_service_style_CharacterPropertiesComplex,
							sUNO_service_style_CharacterPropertiesAsian,

							sUNO_service_drawing_PolyPolygonDescriptor,
							sUNO_service_drawing_ShadowProperties,
							sUNO_service_drawing_RotationDescriptor);

						pSeq = &SvxShape_EdgeServices;
					}
				}
				return *pSeq;
			}
		    case OBJ_MEDIA:
			{
				static uno::Sequence< OUString > *pSeq = 0;
				if( 0 == pSeq )
				{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
					{
						static uno::Sequence< OUString > SvxShape_MediaServices;

						comphelper::ServiceInfoHelper::addToSequence( SvxShape_MediaServices, 2,
							sUNO_service_drawing_MediaShape,
							sUNO_service_drawing_Shape);

						pSeq = &SvxShape_MediaServices;
					}
				}
				return *pSeq;
			}
		}
	}
	else if( mpObj.is() && mpObj->GetObjInventor() == FmFormInventor)
	{
#if OSL_DEBUG_LEVEL > 0
		const sal_uInt16 nIdent = mpObj->GetObjIdentifier();
        OSL_ENSURE( nIdent == OBJ_UNO, "SvxShape::_getSupportedServiceNames: FmFormInventor, but no UNO object?" );
#endif
		static uno::Sequence< OUString > *pSeq = 0;
		if( 0 == pSeq )
		{
//					OGuard aGuard( Application::GetSolarMutex() );
//					if( 0 == pSeq )
			{
				static uno::Sequence< OUString > SvxShape_UnoServices;
				comphelper::ServiceInfoHelper::addToSequence( SvxShape_UnoServices, 2,
					sUNO_service_drawing_ControlShape,
					sUNO_service_drawing_Shape );

				pSeq = &SvxShape_UnoServices;
			}
		}
		return *pSeq;
	}
    OSL_ENSURE( false, "SvxShape::_getSupportedServiceNames: could not determine object type!" );
	uno::Sequence< OUString > aSeq;
	return aSeq;
}

//----------------------------------------------------------------------
sal_Bool SAL_CALL SvxShape::supportsService( const OUString& ServiceName ) throw ( uno::RuntimeException )
{
	Sequence< OUString > SupportedServices( getSupportedServiceNames() );
	const ::rtl::OUString * pArray = SupportedServices.getConstArray();
	const sal_Int32 nCount = SupportedServices.getLength();
	sal_Int32 i;
	for( i = 0; i < nCount; i++ )
		if( *pArray++ == ServiceName )
			return sal_True;
	return sal_False;
}

//----------------------------------------------------------------------

// XGluePointsSupplier
uno::Reference< container::XIndexContainer > SAL_CALL SvxShape::getGluePoints()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );
    uno::Reference< container::XIndexContainer > xGluePoints( mxGluePoints );

	if( mpObj.is() && !xGluePoints.is() )
	{
		uno::Reference< container::XIndexContainer > xNew( SvxUnoGluePointAccess_createInstance( mpObj.get() ), uno::UNO_QUERY );
		mxGluePoints = xGluePoints = xNew;
	}

	return xGluePoints;
}

//----------------------------------------------------------------------

// XChild
uno::Reference< uno::XInterface > SAL_CALL SvxShape::getParent(  )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mpObj.is() && mpObj->getParentOfSdrObject() )
	{
		SdrObjList* pObjList = mpObj->getParentOfSdrObject();
		SdrPage* pPage = dynamic_cast< SdrPage* >(pObjList);

		if(pPage)
		{
			return pPage->getUnoPage();
		}

		if(pObjList->getSdrObjectFromSdrObjList())
		{
			return pObjList->getSdrObjectFromSdrObjList()->getUnoShape();
		}

		{
		    DBG_ERROR( "SvxShape::getParent(  ): unexpected SdrObjList" );
		}
	}

	uno::Reference< uno::XInterface > xParent;
	return xParent;
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::setParent( const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface >& )
	throw(lang::NoSupportException, uno::RuntimeException)
{
	throw lang::NoSupportException();
}

//----------------------------------------------------------------------

/** called from the XActionLockable interface methods on initial locking */
void SvxShape::lock()
{
}

//----------------------------------------------------------------------

/** called from the XActionLockable interface methods on final unlock */
void SvxShape::unlock()
{
}

//----------------------------------------------------------------------

// XActionLockable
sal_Bool SAL_CALL SvxShape::isActionLocked(  ) throw (::com::sun::star::uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	return mnLockCount != 0;
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::addActionLock(  ) throw (::com::sun::star::uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	DBG_ASSERT( mnLockCount < 0xffff, "lock overflow in SvxShape!" );
	mnLockCount++;

	if( mnLockCount == 1 )
		lock();
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::removeActionLock(  ) throw (::com::sun::star::uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	DBG_ASSERT( mnLockCount > 0, "lock underflow in SvxShape!" );
	mnLockCount--;

	if( mnLockCount == 0 )
		unlock();
}

//----------------------------------------------------------------------

void SAL_CALL SvxShape::setActionLocks( sal_Int16 nLock ) throw (::com::sun::star::uno::RuntimeException )
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( (mnLockCount == 0) && (nLock != 0) )
		unlock();

	if( (mnLockCount != 0) && (nLock == 0) )
		lock();

	mnLockCount = (sal_uInt16)nLock;
}

//----------------------------------------------------------------------

sal_Int16 SAL_CALL SvxShape::resetActionLocks(  ) throw (::com::sun::star::uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( mnLockCount != 0 )
		unlock();

	sal_Int16 nOldLocks = (sal_Int16)mnLockCount;
	mnLockCount = 0;

	return nOldLocks;
}

//----------------------------------------------------------------------

/** since polygon shapes can change theire kind during editing, we have
	to recheck it here.
	Circle shapes also change theire kind, but theire all treated equal
	so no update is necessary.
*/
void SvxShape::updateShapeKind()
{
	switch( mpImpl->mnObjId )
	{
		case OBJ_LINE:
		case OBJ_POLY:
		case OBJ_PLIN:
		case OBJ_PATHLINE:
		case OBJ_PATHFILL:
		case OBJ_FREELINE:
		case OBJ_FREEFILL:
		case OBJ_PATHPOLY:
		case OBJ_PATHPLIN:
		{
			const sal_uInt32 nId = mpObj->GetObjIdentifier();

			if( nId != mpImpl->mnObjId )
			{
				mpImpl->mnObjId = nId;

			}
			break;
		}
	};
}

/***********************************************************************
* class SvxShapeText                                                   *
***********************************************************************/
SvxShapeText::SvxShapeText() throw ()
: SvxShape(NULL, aSvxMapProvider.GetMap(SVXMAP_TEXT), aSvxMapProvider.GetPropertySet(SVXMAP_TEXT, GetGlobalDrawObjectItemPool()) ), SvxUnoTextBase( ImplGetSvxUnoOutlinerTextCursorSvxPropertySet() )
{
}

//----------------------------------------------------------------------
SvxShapeText::SvxShapeText( SdrObject* pObject ) throw ()
: SvxShape( pObject, aSvxMapProvider.GetMap(SVXMAP_TEXT), aSvxMapProvider.GetPropertySet(SVXMAP_TEXT, GetGlobalDrawObjectItemPool()) ), SvxUnoTextBase( ImplGetSvxUnoOutlinerTextCursorSvxPropertySet() )
{
	if( pObject )
		SetEditSource( new SvxTextEditSource( pObject, 0, static_cast< uno::XWeak * >( this ) ) );
}

//----------------------------------------------------------------------
SvxShapeText::SvxShapeText( SdrObject* pObject, const SfxItemPropertyMapEntry* pPropertyMap, const SvxItemPropertySet* pPropertySet ) throw ()
: SvxShape( pObject, pPropertyMap, pPropertySet ), SvxUnoTextBase( ImplGetSvxUnoOutlinerTextCursorSvxPropertySet() )
{
	if( pObject )
		SetEditSource( new SvxTextEditSource( pObject, 0, static_cast< uno::XWeak * >( this ) ) );
}

//----------------------------------------------------------------------
SvxShapeText::~SvxShapeText() throw ()
{
	// check if only this instance is registered at the ranges
	DBG_ASSERT( (NULL == GetEditSource()) || (GetEditSource()->getRanges().size()==1),
		"svx::SvxShapeText::~SvxShapeText(), text shape with living text ranges destroyed!");
}

void SvxShapeText::Create( SdrObject* pNewObj, SvxDrawPage* pNewPage )
{
	if( pNewObj && (NULL == GetEditSource()))
		SetEditSource( new SvxTextEditSource( pNewObj, 0, static_cast< uno::XWeak* >(this) ) );

	SvxShape::Create( pNewObj, pNewPage );
}

// XInterface
//----------------------------------------------------------------------
uno::Any SAL_CALL SvxShapeText::queryInterface( const uno::Type & rType )
	throw( uno::RuntimeException )
{
	return SvxShape::queryInterface( rType );
}

//----------------------------------------------------------------------

uno::Any SAL_CALL SvxShapeText::queryAggregation( const uno::Type & rType )
	throw( uno::RuntimeException )
{
	uno::Any aAny( SvxShape::queryAggregation( rType ) );
	if( aAny.hasValue() )
		return aAny;

	return SvxUnoTextBase::queryAggregation( rType );
}

//----------------------------------------------------------------------

void SAL_CALL SvxShapeText::acquire() throw()
{
	SvxShape::acquire();
}

//----------------------------------------------------------------------
void SAL_CALL SvxShapeText::release() throw()
{
	SvxShape::release();
}

// XServiceInfo
//----------------------------------------------------------------------
OUString SAL_CALL SvxShapeText::getImplementationName() throw( uno::RuntimeException )
{
	static OUString aServiceName( RTL_CONSTASCII_USTRINGPARAM("SvxShapeText") );
	return aServiceName;
}

//----------------------------------------------------------------------
uno::Sequence< OUString > SAL_CALL SvxShapeText::getSupportedServiceNames() throw( uno::RuntimeException )
{
	return SvxShape::getSupportedServiceNames();
}

//----------------------------------------------------------------------
sal_Bool SAL_CALL SvxShapeText::supportsService( const OUString& ServiceName ) throw ( uno::RuntimeException )
{
	return SvxShape::supportsService(ServiceName);
}

	// XTypeProvider
//----------------------------------------------------------------------
uno::Sequence< uno::Type > SAL_CALL SvxShapeText::getTypes()
	throw( uno::RuntimeException )
{
	return SvxShape::getTypes();
}

sal_Int64 SAL_CALL SvxShapeText::getSomething( const ::com::sun::star::uno::Sequence< sal_Int8 >& rId ) throw(::com::sun::star::uno::RuntimeException) \
{
	const sal_Int64 nReturn = SvxShape::getSomething( rId );
	if( nReturn )
		return nReturn;

	return SvxUnoTextBase::getSomething( rId );
}

//----------------------------------------------------------------------
uno::Sequence< sal_Int8 > SAL_CALL SvxShapeText::getImplementationId()
	throw( uno::RuntimeException )
{
	static ::cppu::OImplementationId* pID = NULL ;

	if ( pID == NULL )
	{
		// Ready for multithreading; get global mutex for first call of this method only! see before
		MutexGuard aGuard( osl::Mutex::getGlobalMutex() ) ;

		// Control these pointer again ... it can be, that another instance will be faster then these!
		if ( pID == NULL )
		{
			// Create a new static ID ...
			static ::cppu::OImplementationId aID( sal_False ) ;
			// ... and set his address to static pointer!
			pID = &aID ;
		}
	}

	return pID->getImplementationId() ;
}

//----------------------------------------------------------------------

/** called from the XActionLockable interface methods on initial locking */
void SvxShapeText::lock()
{
	SvxTextEditSource* pEditSource = (SvxTextEditSource*)GetEditSource();
	if( pEditSource )
		pEditSource->lock();
}

//----------------------------------------------------------------------

/** called from the XActionLockable interface methods on final unlock */
void SvxShapeText::unlock()
{
	SvxTextEditSource* pEditSource = (SvxTextEditSource*)GetEditSource();
	if( pEditSource )
		pEditSource->unlock();
}

// ::com::sun::star::text::XTextRange
uno::Reference< text::XTextRange > SAL_CALL SvxShapeText::getStart() throw(uno::RuntimeException)
{
    ::vos::OGuard aGuard( Application::GetSolarMutex() );
	SvxTextForwarder* pForwarder = mpEditSource ? mpEditSource->GetTextForwarder() : NULL;
	if( pForwarder )
		::GetSelection( maSelection, pForwarder );
	return SvxUnoTextBase::getStart();

}

uno::Reference< text::XTextRange > SAL_CALL SvxShapeText::getEnd() throw(uno::RuntimeException)
{
    ::vos::OGuard aGuard( Application::GetSolarMutex() );
	SvxTextForwarder* pForwarder = mpEditSource ? mpEditSource->GetTextForwarder() : NULL;
	if( pForwarder )
		::GetSelection( maSelection, pForwarder );
	return SvxUnoTextBase::getEnd();
}

OUString SAL_CALL SvxShapeText::getString() throw(uno::RuntimeException)
{
    ::vos::OGuard aGuard( Application::GetSolarMutex() );
	SvxTextForwarder* pForwarder = mpEditSource ? mpEditSource->GetTextForwarder() : NULL;
	if( pForwarder )
		::GetSelection( maSelection, pForwarder );
	return SvxUnoTextBase::getString();
}


void SAL_CALL SvxShapeText::setString( const OUString& aString ) throw(uno::RuntimeException)
{
    ::vos::OGuard aGuard( Application::GetSolarMutex() );
	SvxTextForwarder* pForwarder = mpEditSource ? mpEditSource->GetTextForwarder() : NULL;
	if( pForwarder )
		::GetSelection( maSelection, pForwarder );
	SvxUnoTextBase::setString( aString );
}

// overide these for special property handling in subcasses. Return true if property is handled
bool SvxShapeText::setPropertyValueImpl( const ::rtl::OUString& rName, const SfxItemPropertySimpleEntry* pProperty, const ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException)
{
	// HACK-fix #99090#
	// since SdrTextObj::SetVerticalWriting exchanges
	// SDRATTR_TEXT_AUTOGROWWIDTH and SDRATTR_TEXT_AUTOGROWHEIGHT,
	// we have to set the textdirection here

	if( pProperty->nWID == SDRATTR_TEXTDIRECTION )
	{
		SdrTextObj* pTextObj = dynamic_cast< SdrTextObj* >( mpObj.get() );
		if( pTextObj )
		{
			com::sun::star::text::WritingMode eMode;
			if( rValue >>= eMode )
			{
				pTextObj->SetVerticalWriting( eMode == com::sun::star::text::WritingMode_TB_RL );
			}
		}
		return true;
	}
    return SvxShape::setPropertyValueImpl( rName, pProperty, rValue );
}

bool SvxShapeText::getPropertyValueImpl( const ::rtl::OUString& rName, const SfxItemPropertySimpleEntry* pProperty, ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException)
{
	if( pProperty->nWID == SDRATTR_TEXTDIRECTION )
	{
		SdrTextObj* pTextObj = dynamic_cast< SdrTextObj* >( mpObj.get() );
		if( pTextObj && pTextObj->IsVerticalWriting() )
            rValue <<= com::sun::star::text::WritingMode_TB_RL;
        else
            rValue <<= com::sun::star::text::WritingMode_LR_TB;
		return true;
	}

    return SvxShape::getPropertyValueImpl( rName, pProperty, rValue );
}

bool SvxShapeText::getPropertyStateImpl( const SfxItemPropertySimpleEntry* pProperty, ::com::sun::star::beans::PropertyState& rState ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::uno::RuntimeException)
{
	return SvxShape::getPropertyStateImpl( pProperty, rState );
}

bool SvxShapeText::setPropertyToDefaultImpl( const SfxItemPropertySimpleEntry* pProperty ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::uno::RuntimeException)
{
	return SvxShape::setPropertyToDefaultImpl( pProperty );
}

/***********************************************************************
* class SvxShapeRect                                                   *
***********************************************************************/
SvxShapeRect::SvxShapeRect( SdrObject* pObj ) throw()
: SvxShapeText( pObj, aSvxMapProvider.GetMap(SVXMAP_SHAPE), aSvxMapProvider.GetPropertySet(SVXMAP_SHAPE, GetGlobalDrawObjectItemPool()))
{
}

SvxShapeRect::~SvxShapeRect() throw()
{
}

uno::Any SAL_CALL SvxShapeRect::queryInterface( const uno::Type & rType ) throw(uno::RuntimeException)
{
	return SvxShapeText::queryInterface( rType );
}

uno::Any SAL_CALL SvxShapeRect::queryAggregation( const uno::Type & rType ) throw(uno::RuntimeException)
{
	return SvxShapeText::queryAggregation( rType );
}

void SAL_CALL SvxShapeRect::acquire() throw()
{
	OWeakAggObject::acquire();
}

void SAL_CALL SvxShapeRect::release() throw()
{
	OWeakAggObject::release();
}
//----------------------------------------------------------------------
// XServiceInfo
//----------------------------------------------------------------------
uno::Sequence< OUString > SvxShapeRect::getSupportedServiceNames(void) throw( uno::RuntimeException )
{
	return SvxShape::getSupportedServiceNames();
}

/** returns a StarOffice API wrapper for the given SdrObject */
uno::Reference< drawing::XShape > GetXShapeForSdrObject( SdrObject* pObj ) throw ()
{
	uno::Reference< drawing::XShape > xShape( pObj->getUnoShape(), uno::UNO_QUERY );
	return xShape;
}

/** returns the SdrObject from the given StarOffice API wrapper */
SdrObject* GetSdrObjectFromXShape( uno::Reference< drawing::XShape > xShape ) throw()
{
    SvxShape* pShape = SvxShape::getImplementation( xShape );
	return pShape ? pShape->GetSdrObject() : 0;
}

//----------------------------------------------------------------------

SdrObject* SdrObject::getSdrObjectFromXShape( const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface >& xInt )
{
    SvxShape* pSvxShape = SvxShape::getImplementation( xInt );
	return pSvxShape ? pSvxShape->GetSdrObject() : 0;
}

uno::Any SvxItemPropertySet_getPropertyValue( const SvxItemPropertySet& rPropSet, const SfxItemPropertySimpleEntry* pMap, const SfxItemSet& rSet )
{
	if(!pMap || !pMap->nWID)
		return uno::Any();

	// Check is for items that store either metric values if thei are positiv or percentage if thei are negativ.
    bool bDontConvertNegativeValues = ( pMap->nWID == XATTR_FILLBMP_SIZEX || pMap->nWID == XATTR_FILLBMP_SIZEY );
	return rPropSet.getPropertyValue( pMap, rSet, (pMap->nWID != SDRATTR_XMLATTRIBUTES), bDontConvertNegativeValues );
}

void SvxItemPropertySet_setPropertyValue( const SvxItemPropertySet& rPropSet, const SfxItemPropertySimpleEntry* pMap, const uno::Any& rVal, SfxItemSet& rSet )
{
	if(!pMap || !pMap->nWID)
		return;

    bool bDontConvertNegativeValues = ( pMap->nWID == XATTR_FILLBMP_SIZEX || pMap->nWID == XATTR_FILLBMP_SIZEY );
	rPropSet.setPropertyValue( pMap, rVal, rSet, bDontConvertNegativeValues );
}

/*
 * aligner_1mm.h
 */

#ifndef ALIGNER_1MM_H_
#define ALIGNER_1MM_H_

#include <utility>
#include <vector>
#include "aligner.h"
#include "hit.h"
#include "range_source.h"
#include "row_chaser.h"
#include "range_chaser.h"
#include "ref_aligner.h"

/**
 * Concrete factory class for constructing unpaired exact aligners.
 */
class Unpaired1mmAlignerV1Factory : public AlignerFactory {
	typedef RangeSourceDriver<EbwtRangeSource> TRangeSrcDr;
	typedef CostAwareRangeSourceDriver<EbwtRangeSource> TCostAwareRangeSrcDr;
	typedef std::vector<TRangeSrcDr*> TRangeSrcDrPtrVec;
public:
	Unpaired1mmAlignerV1Factory(
			Ebwt<String<Dna> >& ebwtFw,
			Ebwt<String<Dna> >* ebwtBw,
			bool doFw,
			bool doRc,
			HitSink& sink,
			const HitSinkPerThreadFactory& sinkPtFactory,
			RangeCache *cacheFw,
			RangeCache *cacheBw,
			uint32_t cacheLimit,
			ChunkPool *pool,
			BitPairReference* refs,
			vector<String<Dna5> >& os,
			bool maqPenalty,
			bool qualOrder,
			bool strandFix,
			bool rangeMode,
			bool verbose,
			bool quiet,
			uint32_t seed) :
			ebwtFw_(ebwtFw),
			ebwtBw_(ebwtBw),
			doFw_(doFw),
			doRc_(doRc),
			sink_(sink),
			sinkPtFactory_(sinkPtFactory),
			cacheFw_(cacheFw),
			cacheBw_(cacheBw),
			cacheLimit_(cacheLimit),
			pool_(pool),
			refs_(refs),
			os_(os),
			maqPenalty_(maqPenalty),
			qualOrder_(qualOrder),
			strandFix_(strandFix),
			rangeMode_(rangeMode),
			verbose_(verbose),
			quiet_(quiet)
	{
		assert(ebwtFw.isInMemory());
		assert(ebwtBw != NULL);
		assert(ebwtBw->isInMemory());
	}

	/**
	 * Create a new UnpairedExactAlignerV1s.
	 */
	virtual Aligner* create() const {

		HitSinkPerThread* sinkPt = sinkPtFactory_.create();
		EbwtSearchParams<String<Dna> >* params =
			new EbwtSearchParams<String<Dna> >(*sinkPt, os_);

		const int halfAndHalf = 0;
		const bool seeded = false;

		EbwtRangeSource *rFw_Bw = new EbwtRangeSource(
			 ebwtBw_, true, OFF_MASK, true,  verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);
		EbwtRangeSource *rFw_Fw = new EbwtRangeSource(
			&ebwtFw_, true, OFF_MASK, false, verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);

		EbwtRangeSourceDriver * drFw_Bw = new EbwtRangeSourceDriver(
			*params, rFw_Bw, true, false, maqPenalty_, qualOrder_, sink_, sinkPt,
			0,          // seedLen (0 = whole read is seed)
			false,      // nudgeLeft (true for Fw index, false for Bw)
			PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
			PIN_TO_LEN, // allow 1 mismatch in rest of read
			PIN_TO_LEN, // "
			PIN_TO_LEN, // "
			os_, verbose_, quiet_, true, pool_, NULL);
		//
		EbwtRangeSourceDriver * drFw_Fw = new EbwtRangeSourceDriver(
			*params, rFw_Fw, true, false, maqPenalty_, qualOrder_, sink_, sinkPt,
			0,          // seedLen (0 = whole read is seed)
			true,       // nudgeLeft (true for Fw index, false for Bw)
			PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
			PIN_TO_LEN, // allow 1 mismatch in rest of read
			PIN_TO_LEN, // "
			PIN_TO_LEN, // "
			os_, verbose_, quiet_, true, pool_, NULL);
		TRangeSrcDrPtrVec *drVec = new TRangeSrcDrPtrVec();
		if(doFw_) {
			drVec->push_back(drFw_Bw);
			drVec->push_back(drFw_Fw);
		}

		EbwtRangeSource *rRc_Fw = new EbwtRangeSource(
			&ebwtFw_, false, OFF_MASK, true,  verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);
		EbwtRangeSource *rRc_Bw = new EbwtRangeSource(
			 ebwtBw_, false, OFF_MASK, false, verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);

		EbwtRangeSourceDriver * drRc_Fw = new EbwtRangeSourceDriver(
			*params, rRc_Fw, false, false, maqPenalty_, qualOrder_, sink_, sinkPt,
			0,          // seedLen (0 = whole read is seed)
			true,       // nudgeLeft (true for Fw index, false for Bw)
			PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
			PIN_TO_LEN, // allow 1 mismatch in rest of read
			PIN_TO_LEN, // "
			PIN_TO_LEN, // "
			os_, verbose_, quiet_, true, pool_, NULL);
		//
		EbwtRangeSourceDriver * drRc_Bw = new EbwtRangeSourceDriver(
			*params, rRc_Bw, false, false, maqPenalty_, qualOrder_, sink_, sinkPt,
			0,          // seedLen (0 = whole read is seed)
			false,      // nudgeLeft (true for Fw index, false for Bw)
			PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
			PIN_TO_LEN, // allow 1 mismatch in rest of read
			PIN_TO_LEN, // "
			PIN_TO_LEN, // "
			os_, verbose_, quiet_, true, pool_, NULL);
		if(doRc_) {
			drVec->push_back(drRc_Fw);
			drVec->push_back(drRc_Bw);
		}
		TCostAwareRangeSrcDr* dr = new TCostAwareRangeSrcDr(strandFix_, drVec, verbose_, quiet_, false);
		delete drVec;

		// Set up a RangeChaser
		RangeChaser<String<Dna> > *rchase =
			new RangeChaser<String<Dna> >(cacheLimit_, cacheFw_, cacheBw_);

		// Set up the aligner
		return new UnpairedAlignerV2<EbwtRangeSource>(
			params, dr, rchase,
			sink_, sinkPtFactory_, sinkPt, os_, refs_,
			rangeMode_, verbose_, quiet_, INT_MAX, pool_, NULL, NULL);
	}

private:
	Ebwt<String<Dna> >& ebwtFw_;
	Ebwt<String<Dna> >* ebwtBw_;
	bool doFw_;
	bool doRc_;
	HitSink& sink_;
	const HitSinkPerThreadFactory& sinkPtFactory_;
	RangeCache *cacheFw_;
	RangeCache *cacheBw_;
	const uint32_t cacheLimit_;
	ChunkPool *pool_;
	BitPairReference* refs_;
	vector<String<Dna5> >& os_;
	const bool maqPenalty_;
	const bool qualOrder_;
	bool strandFix_;
	bool rangeMode_;
	bool verbose_;
	bool quiet_;
};

/**
 * Concrete factory class for constructing unpaired exact aligners.
 */
class Paired1mmAlignerV1Factory : public AlignerFactory {
	typedef RangeSourceDriver<EbwtRangeSource> TRangeSrcDr;
	typedef CostAwareRangeSourceDriver<EbwtRangeSource> TCostAwareRangeSrcDr;
	typedef std::vector<TRangeSrcDr*> TRangeSrcDrPtrVec;
public:
	Paired1mmAlignerV1Factory(
			Ebwt<String<Dna> >& ebwtFw,
			Ebwt<String<Dna> >* ebwtBw,
			bool color,
			bool doFw,
			bool doRc,
			bool v1,
			HitSink& sink,
			const HitSinkPerThreadFactory& sinkPtFactory,
			bool mate1fw,
			bool mate2fw,
			uint32_t peInner,
			uint32_t peOuter,
			bool dontReconcile,
			uint32_t symCeil,
			uint32_t mixedThresh,
			uint32_t mixedAttemptLim,
			RangeCache *cacheFw,
			RangeCache *cacheBw,
			uint32_t cacheLimit,
			ChunkPool *pool,
			BitPairReference* refs,
			vector<String<Dna5> >& os,
			bool reportSe,
			bool maqPenalty,
			bool qualOrder,
			bool strandFix,
			bool rangeMode,
			bool verbose,
			bool quiet,
			uint32_t seed) :
			ebwtFw_(ebwtFw),
			ebwtBw_(ebwtBw),
			color_(color),
			doFw_(doFw),
			doRc_(doRc),
			v1_(v1),
			sink_(sink),
			sinkPtFactory_(sinkPtFactory),
			mate1fw_(mate1fw),
			mate2fw_(mate2fw),
			peInner_(peInner),
			peOuter_(peOuter),
			dontReconcile_(dontReconcile),
			symCeil_(symCeil),
			mixedThresh_(mixedThresh),
			mixedAttemptLim_(mixedAttemptLim),
			cacheFw_(cacheFw),
			cacheBw_(cacheBw),
			cacheLimit_(cacheLimit),
			pool_(pool),
			refs_(refs), os_(os),
			reportSe_(reportSe),
			maqPenalty_(maqPenalty),
			qualOrder_(qualOrder),
			strandFix_(strandFix),
			rangeMode_(rangeMode),
			verbose_(verbose),
			quiet_(quiet)
	{
		assert(ebwtBw != NULL);
		assert(ebwtFw.isInMemory());
		assert(ebwtBw->isInMemory());
	}

	/**
	 * Create a new UnpairedExactAlignerV1s.
	 */
	virtual Aligner* create() const {
		HitSinkPerThread* sinkPt = sinkPtFactory_.createMult(2);
		HitSinkPerThread* sinkPtSe1 = NULL, * sinkPtSe2 = NULL;
		EbwtSearchParams<String<Dna> >* params =
			new EbwtSearchParams<String<Dna> >(*sinkPt, os_);
		EbwtSearchParams<String<Dna> >* paramsSe1 = NULL, * paramsSe2 = NULL;
		if(reportSe_) {
			sinkPtSe1 = sinkPtFactory_.create();
			sinkPtSe2 = sinkPtFactory_.create();
			paramsSe1 =
				new EbwtSearchParams<String<Dna> >(*sinkPtSe1, os_);
			paramsSe2 =
				new EbwtSearchParams<String<Dna> >(*sinkPtSe2, os_);
		}

		const int halfAndHalf = 0;
		const bool seeded = false;

		bool do1Fw = true;
		bool do1Rc = true;
		bool do2Fw = true;
		bool do2Rc = true;
		if(!doFw_) {
			if(mate1fw_) do1Fw = false;
			else         do1Rc = false;
			if(mate2fw_) do2Fw = false;
			else         do2Rc = false;
		}
		if(!doRc_) {
			if(mate1fw_) do1Rc = false;
			else         do1Fw = false;
			if(mate2fw_) do2Rc = false;
			else         do2Fw = false;
		}

		TRangeSrcDrPtrVec *dr1FwVec;
		dr1FwVec = new TRangeSrcDrPtrVec();
		if(do1Fw) {
			EbwtRangeSource *r1Fw_Bw = new EbwtRangeSource(
				 ebwtBw_, true, OFF_MASK, true,  verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);
			EbwtRangeSource *r1Fw_Fw = new EbwtRangeSource(
				&ebwtFw_, true, OFF_MASK, false, verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);

			EbwtRangeSourceDriver * dr1Fw_Bw = new EbwtRangeSourceDriver(
				*params, r1Fw_Bw, true, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen (0 = whole read is seed)
				true,       // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
				PIN_TO_LEN, // allow 1 mismatch in rest of read
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, true, pool_, NULL);
			EbwtRangeSourceDriver * dr1Fw_Fw = new EbwtRangeSourceDriver(
				*params, r1Fw_Fw, true, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen
				false,      // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right-hand half alignment is unrevisitable
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, true, pool_, NULL);

			dr1FwVec->push_back(dr1Fw_Bw);
			dr1FwVec->push_back(dr1Fw_Fw);
		}

		TRangeSrcDrPtrVec *dr1RcVec;
		if(v1_) {
			dr1RcVec = new TRangeSrcDrPtrVec();
		} else {
			dr1RcVec = dr1FwVec;
		}
		if(do1Rc) {
			EbwtRangeSource *r1Rc_Fw = new EbwtRangeSource(
				&ebwtFw_, false, OFF_MASK, true,  verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);
			EbwtRangeSource *r1Rc_Bw = new EbwtRangeSource(
				 ebwtBw_, false, OFF_MASK, false, verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);

			EbwtRangeSourceDriver * dr1Rc_Fw = new EbwtRangeSourceDriver(
				*params, r1Rc_Fw, false, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen
				true,       // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right-hand half alignment is unrevisitable
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, true, pool_, NULL);
			EbwtRangeSourceDriver * dr1Rc_Bw = new EbwtRangeSourceDriver(
				*params, r1Rc_Bw, false, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen (0 = whole read is seed)
				false,      // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
				PIN_TO_LEN, // allow 1 mismatch in rest of read
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, true, pool_, NULL);
			dr1RcVec->push_back(dr1Rc_Fw);
			dr1RcVec->push_back(dr1Rc_Bw);
		}

		TRangeSrcDrPtrVec *dr2FwVec;
		if(v1_) {
			dr2FwVec = new TRangeSrcDrPtrVec();
		} else {
			dr2FwVec = dr1FwVec;
		}
		if(do2Fw) {
			EbwtRangeSource *r2Fw_Bw = new EbwtRangeSource(
				 ebwtBw_, true, OFF_MASK, true,  verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);
			EbwtRangeSource *r2Fw_Fw = new EbwtRangeSource(
				&ebwtFw_, true, OFF_MASK, false, verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);

			EbwtRangeSourceDriver * dr2Fw_Bw = new EbwtRangeSourceDriver(
				*params, r2Fw_Bw, true, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen (0 = whole read is seed)
				true,       // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
				PIN_TO_LEN, // allow 1 mismatch in rest of read
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, false, pool_, NULL);
			EbwtRangeSourceDriver * dr2Fw_Fw = new EbwtRangeSourceDriver(
				*params, r2Fw_Fw, true, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen
				false,      // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right-hand half alignment is unrevisitable
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, false, pool_, NULL);
			dr2FwVec->push_back(dr2Fw_Bw);
			dr2FwVec->push_back(dr2Fw_Fw);
		}

		TRangeSrcDrPtrVec *dr2RcVec;
		if(v1_) {
			dr2RcVec = new TRangeSrcDrPtrVec();
		} else {
			dr2RcVec = dr1FwVec;
		}
		if(do2Rc) {
			EbwtRangeSource *r2Rc_Fw = new EbwtRangeSource(
				&ebwtFw_, false, OFF_MASK, true,  verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);
			EbwtRangeSource *r2Rc_Bw = new EbwtRangeSource(
				 ebwtBw_, false, OFF_MASK, false, verbose_, quiet_, halfAndHalf, seeded, maqPenalty_, qualOrder_);

			EbwtRangeSourceDriver * dr2Rc_Fw = new EbwtRangeSourceDriver(
				*params, r2Rc_Fw, false, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen
				true,       // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right-hand half alignment is unrevisitable
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, false, pool_, NULL);
			EbwtRangeSourceDriver * dr2Rc_Bw = new EbwtRangeSourceDriver(
				*params, r2Rc_Bw, false, false, maqPenalty_, qualOrder_, sink_, sinkPt,
				0,          // seedLen (0 = whole read is seed)
				false,      // nudgeLeft (true for Fw index, false for Bw)
				PIN_TO_HI_HALF_EDGE, // right half is unrevisitable
				PIN_TO_LEN, // allow 1 mismatch in rest of read
				PIN_TO_LEN, // "
				PIN_TO_LEN, // "
				os_, verbose_, quiet_, false, pool_, NULL);
			dr2RcVec->push_back(dr2Rc_Fw);
			dr2RcVec->push_back(dr2Rc_Bw);
		}

		RefAligner<String<Dna5> >* refAligner =
			new OneMMRefAligner<String<Dna5> >(color_, verbose_, quiet_);

		// Set up a RangeChaser
		RangeChaser<String<Dna> > *rchase =
			new RangeChaser<String<Dna> >(cacheLimit_, cacheFw_, cacheBw_);

		if(v1_) {
			PairedBWAlignerV1<EbwtRangeSource>* al = new PairedBWAlignerV1<EbwtRangeSource>(
				params,
				new TCostAwareRangeSrcDr(strandFix_, dr1FwVec, verbose_, quiet_, false),
				new TCostAwareRangeSrcDr(strandFix_, dr1RcVec, verbose_, quiet_, false),
				new TCostAwareRangeSrcDr(strandFix_, dr2FwVec, verbose_, quiet_, false),
				new TCostAwareRangeSrcDr(strandFix_, dr2RcVec, verbose_, quiet_, false),
				refAligner, rchase,
				sink_, sinkPtFactory_, sinkPt, mate1fw_, mate2fw_,
				peInner_, peOuter_, dontReconcile_, symCeil_, mixedThresh_,
				mixedAttemptLim_, refs_, rangeMode_, verbose_,
				quiet_, INT_MAX, pool_, NULL);
			delete dr1FwVec;
			delete dr1RcVec;
			delete dr2FwVec;
			delete dr2RcVec;
			return al;
		} else {
			PairedBWAlignerV2<EbwtRangeSource>* al = new PairedBWAlignerV2<EbwtRangeSource>(
				params, paramsSe1, paramsSe2,
				new TCostAwareRangeSrcDr(strandFix_, dr1FwVec, verbose_, quiet_, true),
				refAligner, rchase,
				sink_, sinkPtFactory_,
				sinkPt, sinkPtSe1, sinkPtSe2,
				mate1fw_, mate2fw_,
				peInner_, peOuter_,
				mixedAttemptLim_, refs_, rangeMode_,
				verbose_, quiet_, INT_MAX, pool_, NULL);
			delete dr1FwVec;
			return al;
		}
	}

private:
	Ebwt<String<Dna> >& ebwtFw_;
	Ebwt<String<Dna> >* ebwtBw_;
	bool color_;
	bool doFw_;
	bool doRc_;
	bool v1_;
	HitSink& sink_;
	const HitSinkPerThreadFactory& sinkPtFactory_;
	const bool mate1fw_;
	const bool mate2fw_;
	const uint32_t peInner_;
	const uint32_t peOuter_;
	const bool dontReconcile_;
	const uint32_t symCeil_;
	const uint32_t mixedThresh_;
	const uint32_t mixedAttemptLim_;
	RangeCache *cacheFw_;
	RangeCache *cacheBw_;
	const uint32_t cacheLimit_;
	ChunkPool *pool_;
	BitPairReference* refs_;
	vector<String<Dna5> >& os_;
	const bool reportSe_;
	const bool maqPenalty_;
	const bool qualOrder_;
	const bool strandFix_;
	const bool rangeMode_;
	const bool verbose_;
	const bool quiet_;
};

#endif /* ALIGNER_1MM_H_ */

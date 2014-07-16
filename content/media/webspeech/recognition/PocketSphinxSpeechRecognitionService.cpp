/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"

#include "PocketSphinxSpeechRecognitionService.h"

#include "SpeechRecognition.h"
#include "SpeechRecognitionAlternative.h"
#include "SpeechRecognitionResult.h"
#include "SpeechRecognitionResultList.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
extern "C"
{
  #include <pocketsphinx/pocketsphinx.h>
  #include <sphinxbase/sphinx_config.h>
}

namespace mozilla {

  using namespace dom;

  NS_IMPL_ISUPPORTS(PocketSphinxSpeechRecognitionService, nsISpeechRecognitionService, nsIObserver)

  PocketSphinxSpeechRecognitionService::PocketSphinxSpeechRecognitionService()
  {
    NS_WARNING("==== CONSTRUCTING  PocketSphinxSpeechRecognitionService === ");



    /*
    nsCOMPtr<nsIFile> tmpAudioFile;
    // get audio temp folder
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpAudioFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
       return;
    }
    rv = tmpAudioFile->Append(NS_LITERAL_STRING("audio.raw") );
    NS_ENSURE_SUCCESS_VOID(rv);
    nsString aStringAudioPath;
    tmpAudioFile->GetPath(aStringAudioPath);
    maudiopath = NS_ConvertUTF16toUTF8(aStringAudioPath);
*/

    config = cmd_ln_init(NULL, ps_args(), TRUE,
               "-hmm", "models/hub4wsj_sc_8k", // acoustic model
//                "-jsgf", mgram , // initial grammar
               "-dict", "models/dict/cmu07a.dic", // point to yours
               NULL);
     if (config == NULL)
       NS_WARNING("ERROR CREATING PSCONFIG");

     ps = ps_init(config);
     if (ps == NULL)
       NS_WARNING("ERROR CREATING PSDECODER");

     NS_WARNING("==== CONSTRUCTED  PocketSphinxSpeechRecognitionService === ");

     mSpeexState = NULL;

  }

  PocketSphinxSpeechRecognitionService::~PocketSphinxSpeechRecognitionService()
  {
    NS_WARNING("==== Destructing PocketSphinxSpeechRecognitionService === ");

    if (config) config = NULL;
    if (ps) ps = NULL;
    if (mSpeexState) mSpeexState = NULL;

    NS_WARNING("==== DESTRUCTED PocketSphinxSpeechRecognitionService === ");

  }

  // CALL START IN JS FALLS HERE
  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::Initialize(WeakPtr<SpeechRecognition> aSpeechRecognition)
  {
    NS_WARNING("==== PocketSphinxSpeechRecognitionService::Initialize  === ");

    if (mSpeexState)
      mSpeexState = NULL;

    mRecognition = aSpeechRecognition;
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC, false);
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC, false);



    NS_WARNING("==== END OF PocketSphinxSpeechRecognitionService::Initialize  === ");

    return NS_OK;
  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::ProcessAudioSegment(int32_t aSampleRate, AudioSegment* aAudioSegment)
  {
    if (!mSpeexState) {
        mSpeexState = speex_resampler_init(1,  44100, 16000,  SPEEX_RESAMPLER_QUALITY_MAX  ,  nullptr);
        NS_WARNING("==== STATE CREATED === ");

        _file = fopen("/tmp/audio.raw", "w");

    }


  //  NS_WARNING("==== RESAMPLING CHUNKS === ");
    aAudioSegment->ResampleChunks(mSpeexState);


    AudioSegment::ChunkIterator iterator(*aAudioSegment);
    while (!iterator.IsEnded()) {
      //NS_WARNING("==== START ITERATING === ");

      const int16_t* audio_data = static_cast<const int16_t*>(iterator->mChannelData[0]);

      fwrite(audio_data,sizeof(int16_t) , iterator->mDuration , _file);

      //NS_WARNING("==== END ITERATING === ");
      iterator.Next();
    }


    return NS_OK;
  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::SoundEnd()
  {
    NS_WARNING("==== SOUNDEND() ==== ");
    fclose(_file);

    // Declare javascript result events
    nsRefPtr<SpeechEvent> event =
      new SpeechEvent(mRecognition,
                      SpeechRecognition::EVENT_RECOGNITIONSERVICE_FINAL_RESULT);
    SpeechRecognitionResultList* resultList = new SpeechRecognitionResultList(mRecognition);
    SpeechRecognitionResult* result = new SpeechRecognitionResult(mRecognition);
    SpeechRecognitionAlternative* alternative = new SpeechRecognitionAlternative(mRecognition);
    nsString hypoValue;


    NS_WARNING("==== SOUNDEND() DESTROYING SPEEX STATE ==== ");
    speex_resampler_destroy(mSpeexState);
    mSpeexState= NULL;


    NS_WARNING("==== SOUNDEND() DECODING SPEECH. OPENING FILE === ");
    _file = fopen("/tmp/audio.raw", "r");
    NS_WARNING("==== SOUNDEND() DECODING RAW === ");
    const char *hyp, *uttid;
    int32 score;
    int _psrv = ps_decode_raw(ps, _file, NULL, -1);
    if (_psrv < 0)
    {
      NS_WARNING("ERROR ps_decode_raw");
    }
    fclose(_file);

    NS_WARNING("==== SOUNDEND() GETTING HYP() === ");
    hyp = ps_get_hyp(ps, &score, &uttid);

    if (hyp == NULL) {
      NS_WARNING("ERROR hyp()");
      hypoValue.AssignASCII("");
    } else {
      NS_WARNING("OK hyp(): ");
      NS_WARNING(hyp);
      hypoValue.AssignASCII(hyp);
    }


    NS_WARNING("==== RAISING FINAL RESULT EVENT TO JAVASCRIPT ==== ");
    alternative->mTranscript =   hypoValue;
    alternative->mConfidence = 0.0f;

    result->mItems.AppendElement(alternative);
    resultList->mItems.AppendElement(result);

    event->mRecognitionResultList = resultList;
    NS_DispatchToMainThread(event);


    return NS_OK;
  }


  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::SetGrammarList(WeakPtr<SpeechGrammarList> aSpeechGramarList)
  {

    /*
        // get grammar temp folder
        nsCOMPtr<nsIFile> tmpGramFile;
        nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpGramFile));
        if (NS_WARN_IF(NS_FAILED(rv))) {
           return;
        }
        rv = tmpGramFile->Append(NS_LITERAL_STRING("grm.jsgf") );
        NS_ENSURE_SUCCESS_VOID(rv);
        nsString aStringPath;
        tmpGramFile->GetPath(aStringPath);
        nsCString mgrammarpath = NS_ConvertUTF16toUTF8(aStringPath);
        const char * mgram = mgrammarpath.get();

        NS_WARNING("==== Defined grammar path === ");



     jsgf_rule_iter_t *itor;
     jsgf_t * gram = jsgf_parse_file(grampath.c_str(), NULL);
     jsgf_rule_t * rule;

     for (itor = jsgf_rule_iter(gram); itor; itor = jsgf_rule_iter_next(itor)) {
         rule = jsgf_rule_iter_rule(itor);
         if (jsgf_rule_public(rule))
             break;
     }

     fsg_model_t * m = jsgf_build_fsg(gram, rule, ps_get_logmath(ps), 6.5);
     fsg_set_t* fsgset = ps_get_fsgset(ps);
     fsg_set_add(fsgset, dtgram , m);
     fsg_set_select(fsgset , dtgram );
     ps_update_fsgset(ps);

    return NS_OK;

    */

  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::Abort()
  {
    return NS_OK;
  }

  NS_IMETHODIMP
  PocketSphinxSpeechRecognitionService::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
  {
    NS_WARNING("==== OBSERVE  ==== ");

    MOZ_ASSERT(mRecognition->mTestConfig.mFakeRecognitionService,
               "Got request to fake recognition service event, but "
               TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE " is not set");

    if (!strcmp(aTopic, SPEECH_RECOGNITION_TEST_END_TOPIC)) {
      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC);
      obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC);

      return NS_OK;
    }

    const nsDependentString eventName = nsDependentString(aData);

    if (eventName.EqualsLiteral("EVENT_RECOGNITIONSERVICE_ERROR")) {
      mRecognition->DispatchError(SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
                                  SpeechRecognitionErrorCode::Network, // TODO different codes?
                                  NS_LITERAL_STRING("RECOGNITIONSERVICE_ERROR test event"));

    } else if (eventName.EqualsLiteral("EVENT_RECOGNITIONSERVICE_FINAL_RESULT")) {
      nsRefPtr<SpeechEvent> event =
        new SpeechEvent(mRecognition,
                        SpeechRecognition::EVENT_RECOGNITIONSERVICE_FINAL_RESULT);

      event->mRecognitionResultList = BuildMockResultList();
      NS_DispatchToMainThread(event);
    }

    return NS_OK;
    }

    SpeechRecognitionResultList* PocketSphinxSpeechRecognitionService::BuildMockResultList()
    {
      SpeechRecognitionResultList* resultList = new SpeechRecognitionResultList(mRecognition);
      SpeechRecognitionResult* result = new SpeechRecognitionResult(mRecognition);
      SpeechRecognitionAlternative* alternative = new SpeechRecognitionAlternative(mRecognition);

      alternative->mTranscript = NS_LITERAL_STRING("Mock final result");
      alternative->mConfidence = 0.0f;

      result->mItems.AppendElement(alternative);
      resultList->mItems.AppendElement(result);

      return resultList;
    }

} // namespace mozilla

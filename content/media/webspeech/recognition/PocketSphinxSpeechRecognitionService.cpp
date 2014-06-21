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


extern "C"
{
  #include <pocketsphinx/pocketsphinx.h>
  #include <sphinxbase/sphinx_config.h>
}

namespace mozilla {

using namespace dom;

NS_IMPL_ISUPPORTS(PocketSphinxRecognitionService, nsISpeechRecognitionService, nsIObserver)

PocketSphinxRecognitionService::PocketSphinxRecognitionService()
{
}

PocketSphinxRecognitionService::~PocketSphinxRecognitionService()
{
}

NS_IMETHODIMP
PocketSphinxRecognitionService::Initialize(WeakPtr<SpeechRecognition> aSpeechRecognition)
{
  mRecognition = aSpeechRecognition;
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->AddObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC, false);
  obs->AddObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC, false);

  cmd_ln_t *config = cmd_ln_init(NULL, ps_args(), TRUE,
             "-hmm", "/var/modelsps/hmm/hub4wsj_sc_8k/", // point to yours
             "-jsgf", "/var/www/speechrtc/voiceserver/gramjsgf.jsgf", // point to yours
             "-dict", "/usr/local/share/pocketsphinx/model/lm/en_US/cmu07a.dic", // point to yours
             NULL);
     if (config == NULL) NS_WARNING("ERROR CREATING PSCONFIG");
     ps_decoder_t * ps = ps_init(config);
     if (ps == NULL) NS_WARNING("ERROR CREATING PSDECODER");

  return NS_OK;
}

NS_IMETHODIMP
PocketSphinxRecognitionService::ProcessAudioSegment(AudioSegment* aAudioSegment)
{

  /*

            FILE *_file = fopen(dtdepois, "rb");
            char const *hyp, *uttid;
            int32 score;
            int rv = ps_decode_raw(ps, _file, dtdepois, -1);
            if (rv < 0) puts("error ps_decode_raw");
            hyp = ps_get_hyp(ps, &score, &uttid);

            if (hyp == NULL) {
                puts("Error hyp()");
                write(sock, "ERR", strlen("ERR"));
            } else {
                printf("Recognized: %s\n", hyp);
                // envia final hypothesis
                write(sock, hyp, strlen(hyp));
            }
            fclose(_file);
   */


  return NS_OK;
}

NS_IMETHODIMP
PocketSphinxRecognitionService::SoundEnd()
{
  return NS_OK;
}

NS_IMETHODIMP
PocketSphinxRecognitionService::Abort()
{
  return NS_OK;
}

NS_IMETHODIMP
PocketSphinxRecognitionService::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
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

SpeechRecognitionResultList*
PocketSphinxRecognitionService::BuildMockResultList()
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

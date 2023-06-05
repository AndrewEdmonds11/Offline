void digiGain(std::string detector, std::string beam_state = "", int rebin_factor = 15)
{
  std::string filename;
  TFile* data;
  TH1D* adcSpectrum;

  // Explicit beam ON or beam OFF spectra
  if (beam_state == "ON") {
    filename = "stmDigisSpectrum.root";
    data = new TFile(filename.c_str(), "READ");
    adcSpectrum = (TH1D*) data->Get("plotSTMDigisSpectrum/adcSpectrumOn");
  }
  else if (beam_state == "OFF") {
    filename = "stmDigisSpectrum.root";
    data = new TFile(filename.c_str(), "READ");
    adcSpectrum = (TH1D*) data->Get("plotSTMDigisSpectrum/adcSpectrumOff");
  }
  else {
    filename = "stmDigisSpectrum.root";
    data = new TFile(filename.c_str(), "READ");
    adcSpectrum = (TH1D*) data->Get("plotSTMDigisSpectrum/adcSpectrum");
  }

  // Standard rebin of 10, can change
  // Goal is to have five peaks show up, representing the 511 keV, Cs137, Y88 and 2x Co60
  adcSpectrum->Rebin(rebin_factor);

  // Declaring the energy peak vector
  std::vector<double> adcPeaks;

  // Finding the location of each peak
  const int n_peaks = 9; // May need to modify to allow TSpectrum to find peaks
  int peak_range_check = 0;
  TSpectrum* spectrum = new TSpectrum(n_peaks);
  int n_found_peaks = spectrum->Search(adcSpectrum);
  for (int i_peak = 0; i_peak < n_found_peaks; ++i_peak)
    {
      double peak_x_pos = *(spectrum->GetPositionX() + i_peak);
      std::cout << "Peak number " << i_peak << ": " << peak_x_pos << std::endl;
      if (peak_x_pos >= 900.0) // Threshold to avoid low energy junk peaks
        adcPeaks.push_back(peak_x_pos);
        peak_range_check++;
    }

  if (peak_range_check < 5)
    {
      std::cerr << "\nNot enough fittable peaks in data range\n";
      exit(1);
    }

  // Initializing the output file
  ofstream out;
  out.open("adcPeaks.log", ios::out | ios::trunc);
  out << "peak_number, par_name, par_value, par_error" << std::endl;

  // Plotting the adc spectrum
  adcSpectrum->Draw("HIST");

  // Estimating background using TSpectrum
  TH1* hb = spectrum->Background(adcSpectrum, 20, "SAME");

  TF1* fline = new TF1("fline", "pol1", 0, 5000);
  adcSpectrum->Fit(fline, "qn");

  //adcSpectrum->Add(hb,-1);
  std::cout << "Detector: " << detector << std::endl;

  // Good fits error handling
  int good_fits = 0;

  //Fitting
  for (int j = 0; j < adcPeaks.size(); ++j)
    {
      TF1* fitGaus;
      TString fname(Form("fgaus_%d", j));
      if (detector == "H") // HPGe peak widths are approximately 50 ADC samples
        fitGaus = new TF1(fname, "[0]*TMath::Gaus(x,[1],[2])+[3]*x+[4] + [5]*TMath::Gaus(x,[6],[7])",adcPeaks[j]-50,adcPeaks[j]+50);
      else if (detector == "L") // LaBr peak widths are approximately 250 ADC samples
        fitGaus = new TF1(fname, "[0]*TMath::Gaus(x,[1],[2])+[3]*x+[4]",adcPeaks[j]-250,adcPeaks[j]+250);
      fitGaus->SetParName(0,"Amplitude");
      fitGaus->SetParName(1,"Mean");
      fitGaus->SetParName(2,"Sigma");
      fitGaus->SetParName(3,"Linear");
      fitGaus->SetParName(4,"Constant");

      // Guessing the height for each TSpectrum peak
      int bin = adcSpectrum->FindBin(adcPeaks[j]);
      double guessHeight = adcSpectrum->GetBinContent(bin);
      std::cout << "TSpectrum value for peak " << j  <<  ": " << int (adcPeaks[j]) << std::endl;
      std::cout << "Guess for height of peak " << j  <<  ": " << guessHeight       << std::endl;

      // Guess for FWHM
      double bkrndHeight = hb->GetBinContent(bin);
      double sgnlHeight = guessHeight - bkrndHeight;
      double halfMax = sgnlHeight/2.0;
      double x1, x2;
      bool threshold = false;
      //std::cout << "Half Max: " << halfMax << std::endl;
      for (int k = bin - 5; k < bin + 5; ++k)
        {
          double k_content = adcSpectrum->GetBinContent(k) - hb->GetBinContent(k);
          //std::cout << "k_content: " << k_content << std::endl;
          if (k_content > halfMax && threshold == false)
            {
              x1 = adcSpectrum->GetBinCenter(k);
              threshold = true;
              //std::cout << "x1: " << x1 << std::endl;
            }
          if (k_content < halfMax && threshold == true)
            {
              x2 = adcSpectrum->GetBinCenter(k);
              //std::cout << "x2: " << x2 << std::endl;
              break;
            }
        }
      double FWHM = x2 - x1;
      //std::cout << "FWHM: " << FWHM << std::endl;
      double sigma = FWHM/2.35;
      std::cout << "Sigma: " << sigma << std::endl;

      fitGaus->SetParameters(guessHeight,adcPeaks[j],sigma,-0.05,2); // Setting the parameters with automated guesses
      fitGaus->SetParLimits(1,adcPeaks[j]-10,adcPeaks[j]+10);        // Set limits on peak location
      //fitGaus->SetParLimits(0,0,200);                              // Set limits on fitted heights
      //fitGaus->SetParLimits(2,0.001,10);                           // Set limits on fitted widths
      gStyle->SetOptFit(100);                                        // Want chi2 / ndof

      // If the found peak is less than 1000 ADC sample then don't fit
      if((adcPeaks[j] > 1000) && (adcPeaks[j] < 5000)) {
        TFitResultPtr fitresult = adcSpectrum->Fit(fitGaus, "RS+");
        int n_par = fitresult->NPar();
        std::cout << "Fit status = " << fitresult->Status() << std::endl; // Confirm that the fit converged
        //Draw fitted peaks
        if (fitresult->Status() == 0)
          {
            fitGaus->Draw("LSAME");
            good_fits++;
          }
        // Export fit parameters to a file
        if ( (fitresult->Status() == 0) && (fitresult->Parameter(2) > 0) && (fitresult->Parameter(0) > 0) ) // Export only if the std is positive
            {
            for (int i_par = 0; i_par < n_par; i_par++)
              {
                out   << j << ", "
                      << fitresult->GetParameterName(i_par)
                      << ", " << fitresult->Parameter(i_par)
                      << ", " << fitresult->ParError(i_par)
                      << std::endl;
              }
            }
      }
    }
  if (good_fits != 5)
    {
    std::cerr << "\nError: Improper number of successful fits\n";
    //exit(1);
    }
  out.close();
}

#ifndef DISANA_COMPARER_H
#define DISANA_COMPARER_H

// ROOT headers
#include <TCanvas.h>
#include <TLegend.h>

// STL headers
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Project-specific headers
#include <chrono>

#include "DISANAplotter.h"
#include "DrawStyle.h"

namespace fs = std::filesystem;
// color palettes for different models
std::vector<std::tuple<double, double, double>> modelShades = {
    {0.20, 0.30, 0.85},  // Blue
    {0.90, 0.45, 0.10},  // Orange
    {0.00, 0.60, 0.60},  // Teal green
    {0.00, 0.70, 0.00},  // Green
    {0.60, 0.30, 0.80},  // Purple
    {0.85, 0.10, 0.25},  // Red
    {0.40, 0.40, 0.40}   // Gray (fallback)
};

class DISANAcomparer {
 public:
  // Set the bin ranges used for cross-section calculations and plotting
  void SetXBinsRanges(BinManager bins) { fXbins = bins; }

  void NormalizeHistogram(TH1* hist) {
    if (!hist) return;
    double integral = hist->Integral();
    if (integral > 0) hist->Scale(1.0 / integral);
  }
  // Add a new model with its DataFrame, label, and beam energy
  void AddModelwithPi0Corr(ROOT::RDF::RNode df_dvcs_data, ROOT::RDF::RNode df_pi0_data, ROOT::RDF::RNode df_dvcs_mc, ROOT::RDF::RNode df_pi0_mc, const std::string& label,
                           double beamEnergy, bool fCorrection = false) {
    auto plotter = std::make_unique<DISANAplotter>(df_dvcs_data, beamEnergy, df_pi0_data, df_dvcs_mc, df_pi0_mc);
    std::cout << "Adding model: " << label << " with beam energy: " << beamEnergy << " GeV with Pi0 Correction: " << fCorrection << std::endl;
    plotter->SetPlotApplyCorrection(fCorrection);
    plotter->GenerateKinematicHistos("el");
    plotter->GenerateKinematicHistos("pro");
    plotter->GenerateKinematicHistos("pho");
    labels.push_back(label);
    plotters.push_back(std::move(plotter));
  }

  void AddModel(ROOT::RDF::RNode df, const std::string& label, double beamEnergy) {
    auto plotter = std::make_unique<DISANAplotter>(df, beamEnergy);
    std::cout << "Adding model: " << label << " with beam energy: " << beamEnergy << " GeV without Pi0 Correction" << std::endl;
    plotter->GenerateKinematicHistos("el");
    plotter->GenerateKinematicHistos("pro");
    plotter->GenerateKinematicHistos("pho");
    labels.push_back(label);
    plotters.push_back(std::move(plotter));
  }

  // Set the output directory for saving plots
  void SetOutputDir(const std::string& outdir) {
    outputDir = outdir;
    if (!fs::exists(outputDir)) {
      fs::create_directories(outputDir);
    }
  }

  // Enable or disable individual variable plotting
  void PlotIndividual(bool plotInd) { plotIndividual = plotInd; }

  // Set plot styles for various plot types
  void SetKinStyle(const DrawStyle& style) { styleKin_ = style; }
  void SetDVCSStyle(const DrawStyle& style) { styleDVCS_ = style; }
  void SetCrossSectionStyle(const DrawStyle& style) { styleCrossSection_ = style; }
  void SetBSAStyle(const DrawStyle& style) { styleBSA_ = style; }

  // Enable or disable correctio
  void SetApplyCorrection(bool apply) { applyCorrection = apply; }

  // Load correction histogram from ROOT file
  void LoadCorrectionHistogram(const std::string& filename, const std::string& histoname = "h_correction") {
    correctionHist = nullptr;  // Reset after applying to avoid reusing the same histogram
    TFile* f = TFile::Open(filename.c_str(), "READ");

    if (!f || f->IsZombie()) {
      std::cerr << "Error: Cannot open correction file: " << filename << "\n";
      return;
    }

    correctionHist = dynamic_cast<THnSparseD*>(f->Get(histoname.c_str()));
    if (!correctionHist) {
      std::cerr << "Error: Correction histogram '" << histoname << "' not found in file: " << filename << "\n";
      return;
    }

    // correctionHist->SetDirectory(0);  // Detach from file
    f->Close();
    delete f;
    std::cout << "✅ Correction histogram loaded: " << histoname << "\n";
  }
  /// get mean values of Q^2 and x_B
  std::vector<std::vector<std::vector<std::tuple<double, double, double>>>> getMeanQ2xBt(const BinManager& bins, std::unique_ptr<DISANAplotter>& plotter) {
    const auto& xb_bins = bins.GetXBBins();
    const auto& q2_bins = bins.GetQ2Bins();
    const auto& t_bins = bins.GetTBins();

    size_t n_xb = xb_bins.size() - 1;
    size_t n_q2 = q2_bins.size() - 1;
    size_t n_t = t_bins.size() - 1;

    auto rdf = plotter->GetRDF();

    std::vector<std::vector<std::vector<std::tuple<double, double, double>>>> result(
        n_xb, std::vector<std::vector<std::tuple<double, double, double>>>(n_q2, std::vector<std::tuple<double, double, double>>(n_t)));

    for (size_t ix = 0; ix < n_xb; ++ix) {
      for (size_t iq = 0; iq < n_q2; ++iq) {
        for (size_t it = 0; it < n_t; ++it) {
          double xb_lo = xb_bins[ix], xb_hi = xb_bins[ix + 1];
          double q2_lo = q2_bins[iq], q2_hi = q2_bins[iq + 1];
          double t_lo = t_bins[it], t_hi = t_bins[it + 1];

          // Apply filter
          auto rdf_cut = rdf.Filter(Form("xB >= %f && xB < %f", xb_lo, xb_hi)).Filter(Form("Q2 >= %f && Q2 < %f", q2_lo, q2_hi)).Filter(Form("t >= %f && t < %f", t_lo, t_hi));

          // Compute means
          double mean_xB = rdf_cut.Mean("xB").GetValue();
          double mean_Q2 = rdf_cut.Mean("Q2").GetValue();
          double mean_t = rdf_cut.Mean("t").GetValue();

          result[ix][iq][it] = std::make_tuple(mean_xB, mean_Q2, mean_t);
        }
      }
    }

    return result;
  }

  // Plot all basic kinematic distributions (p, theta, phi) for all particle types
  void PlotKinematicComparison() {
    TCanvas* canvas = new TCanvas("KinematicComparison", "Kinematic Comparison", 1800, 1200);
    canvas->Divide(3, 3);

    std::vector<std::string> types = {"el", "pro", "pho"};
    std::vector<std::string> vars = {"p", "theta", "phi"};

    int pad = 1;
    for (const auto& type : types) {
      for (const auto& var : vars) {
        PlotVariableComparison(type, var, pad++, canvas);
      }
    }

    canvas->Update();
    canvas->SaveAs((outputDir + "KinematicComparison.pdf").c_str());

    // Optionally save individual plots
    if (plotIndividual) {
      for (const auto& type : types) {
        for (const auto& var : vars) {
          PlotSingleVariableComparison(type, var);
        }
      }
    }

    std::cout << "Saved kinematic comparison plots to: " << outputDir + "/KinematicComparison.pdf" << std::endl;
    delete canvas;
  }

  // Plot one specific variable (e.g., p) for a given particle type (e.g., "el")
  void PlotVariableComparison(const std::string& type, const std::string& var, int padIndex, TCanvas* canvas) {
    canvas->cd(padIndex);
    std::string hname_target = "rec" + type + "_" + var;

    TLegend* legend = new TLegend(0.6, 0.7, 0.88, 0.88);
    legend->SetBorderSize(0);
    legend->SetFillStyle(0);

    bool first = true;
    styleKin_.StylePad((TPad*)gPad);

    for (size_t i = 0; i < plotters.size(); ++i) {
      const auto& histograms = plotters[i]->GetAllHistograms();
      TH1* target = nullptr;

      for (TH1* h : histograms) {
        if (std::string(h->GetName()) == hname_target) {
          target = h;
          break;
        }
      }

      if (!target) {
        std::cerr << "[PlotVariableComparison]: Histogram " << hname_target << " not found for model [" << labels[i] << "]\n";
        continue;
      }
      NormalizeHistogram(target);
      styleKin_.StyleTH1(target);
      target->SetLineColor(i + 2);
      target->SetTitle(Form("%s;%s;Count", typeToParticle[type].c_str(), VarName[var].c_str()));

      if (first) {
        target->Draw("HIST");
        first = false;
      } else {
        target->Draw("HIST SAME");
      }

      legend->AddEntry(target, labels[i].c_str(), "l");
    }

    legend->Draw();
  }

  // Save an individual variable comparison plot as PNG
  void PlotSingleVariableComparison(const std::string& type, const std::string& var) {
    TCanvas* canvas = new TCanvas(("c_" + type + "_" + var).c_str(), ("Comparison " + type + " " + var).c_str(), 800, 600);
    gPad->SetGrid();

    std::string hname_target = "rec" + type + "_" + var;

    TLegend* legend = new TLegend(0.6, 0.7, 0.88, 0.88);
    legend->SetBorderSize(0);
    legend->SetFillStyle(0);

    bool first = true;

    for (size_t i = 0; i < plotters.size(); ++i) {
      const auto& histograms = plotters[i]->GetAllHistograms();
      TH1* target = nullptr;

      for (TH1* h : histograms) {
        if (std::string(h->GetName()) == hname_target) {
          target = h;
          break;
        }
      }

      if (!target) {
        std::cerr << "[PlotSingleVariableComparison]: Histogram " << hname_target << " not found for model [" << labels[i] << "]\n";
        continue;
      }

      target->SetLineColor(i + 1);

      if (first) {
        target->Draw("HIST");
        first = false;
      } else {
        target->Draw("HIST SAME");
      }

      legend->AddEntry(target, labels[i].c_str(), "l");
    }

    legend->Draw();
    canvas->Update();
    canvas->SaveAs((outputDir + "/compare_" + type + "_" + var + ".pdf").c_str());
    delete canvas;
  }

  void PlotDVCSKinematicsComparison(bool plotIndividual = false) {
    // Store current global TGaxis state
    int oldMaxDigits = TGaxis::GetMaxDigits();

    std::vector<std::string> variables = {"Q2", "xB", "t", "W", "phi"};
    std::map<std::string, std::string> titles = {{"Q2", "Q^{2} [GeV^{2}]"}, {"xB", "x_{B}"}, {"t", "-t [GeV^{2}]"}, {"W", "W [GeV]"}, {"phi", "#phi [deg]"}};

    TCanvas* canvas = new TCanvas("DVCSVars", "DVCS Kinematic Comparison", 1800, 1400);
    canvas->Divide(3, 2);

    int pad = 1;
    for (const auto& var : variables) {
      canvas->cd(pad++);
      styleDVCS_.StylePad((TPad*)gPad);

      TLegend* legend = new TLegend(0.6, 0.7, 0.88, 0.88);
      legend->SetBorderSize(0);
      legend->SetFillStyle(0);

      bool first = true;
      std::vector<TH1D*> histos_to_draw;

      for (size_t i = 0; i < plotters.size(); ++i) {
        auto rdf = plotters[i]->GetRDF();
        if (!rdf.HasColumn(var)) {
          std::cerr << "[ERROR] Column " << var << " not found in RDF for model " << labels[i] << "\n";
          continue;
        }

        double min = *(rdf.Min(var));
        double max = *(rdf.Max(var));
        if (min == max) {
          min -= 0.1;
          max += 0.1;
        }
        double margin = std::max(1e-3, 0.05 * (max - min));

        // Get histogram (RResultPtr) and clone it
        auto htmp = rdf.Histo1D({Form("h_%s_%zu", var.c_str(), i), titles[var].c_str(), 100, min - margin, max + margin}, var);
        auto h = (TH1D*)htmp->Clone(Form("h_%s_%zu_clone", var.c_str(), i));

        if (!h) continue;  // guard against failed clone

        h->SetDirectory(0);  // prevent ROOT from managing ownership
        NormalizeHistogram(h);
        styleDVCS_.StyleTH1(h);
        h->SetLineColor(i + 2);
        h->SetLineWidth(1);
        h->GetXaxis()->SetTitle(titles[var].c_str());
        h->GetYaxis()->SetTitle("Counts");

        histos_to_draw.push_back(h);
        legend->AddEntry(h, labels[i].c_str(), "l");
      }

      for (size_t j = 0; j < histos_to_draw.size(); ++j) {
        histos_to_draw[j]->Draw(j == 0 ? "HIST" : "HIST SAME");
      }

      if (!histos_to_draw.empty()) {
        legend->Draw();
      }

      if (plotIndividual && (var == "xB" || var == "Q2" || var == "t" || var == "W" || var == "phi")) {
        PlotSingleVariableComparison("el", var);
      }
    }
    canvas->cd(pad);
    auto rdf = plotters.front()->GetRDF();
    auto h2d = rdf.Histo2D({"h_Q2_vs_xB", "Q^{2} vs x_{B};x_{B};Q^{2} [GeV^{2}]", 60, 0, 1.0, 60, 0, 10.0}, "xB", "Q2");

    styleDVCS_.StylePad((TPad*)gPad);
    gPad->SetRightMargin(0.16);
    h2d->GetYaxis()->SetNoExponent(true);
    h2d->SetStats(0);
    h2d->SetTitle("");
    h2d->GetYaxis()->SetLabelFont(42);
    h2d->GetYaxis()->SetLabelSize(0.06);
    h2d->GetYaxis()->SetTitleOffset(1.0);
    h2d->GetYaxis()->SetTitleSize(0.06);
    h2d->GetYaxis()->SetNdivisions(410);

    h2d->GetXaxis()->SetTitleSize(0.065);
    h2d->GetXaxis()->SetLabelFont(42);
    h2d->GetXaxis()->SetLabelSize(0.06);
    h2d->GetXaxis()->SetTitleOffset(0.9);
    h2d->GetXaxis()->SetNdivisions(205);

    h2d->GetZaxis()->SetNdivisions(410);
    h2d->GetZaxis()->SetLabelSize(0.06);
    h2d->GetZaxis()->SetTitleOffset(1.5);
    h2d->GetZaxis()->SetTitleSize(0.06);
    TGaxis::SetMaxDigits(3);
    h2d->DrawCopy("COLZ");
    // Final save and cleanup
    canvas->SaveAs((outputDir + "/DVCS_Kinematics_Comparison.pdf").c_str());
    std::cout << "Saved DVCS kinematics comparison to: " << outputDir + "/DVCS_Kinematics_Comparison.pdf" << std::endl;
    delete canvas;
    TGaxis::SetMaxDigits(oldMaxDigits);
  }
  /// For exclusivity cuts, you can use the following function to select one triplet
  void PlotExclusivityComparisonByDetectorCases(const std::vector<std::pair<std::string, std::string>>& detectorCuts) {
    std::vector<std::tuple<std::string, std::string, std::string, double, double>> vars = {
        {"Mx2_ep", "Missing Mass Squared (ep)", "MM^{2}(ep) [GeV^{2}]", -2.0, 2.0},
        {"Emiss", "Missing Energy", "E_{miss} [GeV]", -2, 3.0},
        {"PTmiss", "Transverse Missing Momentum", "P_{T}^{miss} [GeV/c]", -1.0, 1.0},
        {"Theta_gamma_gamma", "#theta(#gamma, #vec{q})", "#theta_{#gamma#gamma'} [deg]", -10.0, 30},
        {"DeltaPhi", "Coplanarity Angle", "#Delta#phi [deg]", 0, 90},
        {"Mx2_epg", "Missing Mass Squared (ep#gamma)", "MM^{2}(ep#gamma) [GeV^{2}]", -1.0, 1.0},
        {"Mx2_eg", "Invariant Mass (e#gamma)", "M^{2}(e#gamma) [GeV^{2}]", -5.5, 5.5},
        {"Theta_e_gamma", "Angle: e-#gamma", "#theta(e, #gamma) [deg]", 0.0, 180.0},
        {"DeltaE", "Energy Balance", "#DeltaE [GeV]", -2.0, 4.0},
    };

    for (const auto& [cutExpr, cutLabel] : detectorCuts) {
      std::string cleanName = cutLabel;
      std::replace(cleanName.begin(), cleanName.end(), ' ', '_');
      std::replace(cleanName.begin(), cleanName.end(), ',', '_');

      TCanvas* canvas = new TCanvas(("c_" + cleanName).c_str(), cutLabel.c_str(), 1800, 1200);
      int cols = 3;
      int rows = (vars.size() + cols - 1) / cols;
      canvas->Divide(cols, rows);

      for (size_t i = 0; i < vars.size(); ++i) {
        canvas->cd(i + 1);
        const auto& [var, title, xlabel, xmin, xmax] = vars[i];
        gPad->SetTicks();
        styleKin_.StylePad((TPad*)gPad);

        TLegend* legend = new TLegend(0.6, 0.7, 0.88, 0.88);
        legend->SetBorderSize(0);
        legend->SetFillStyle(0);
        legend->SetTextSize(0.04);

        bool first = true;

        for (size_t m = 0; m < plotters.size(); ++m) {
          auto rdf_cut = plotters[m]->GetRDF().Filter(cutExpr, cutLabel);
          if (!rdf_cut.HasColumn(var)) continue;

          auto h = rdf_cut.Histo1D({Form("h_%s_%s_%zu", var.c_str(), cleanName.c_str(), m), (title + ";" + xlabel + ";Counts").c_str(), 100, xmin, xmax}, var);
          h.GetValue();

          TH1D* h_clone = (TH1D*)h.GetPtr()->Clone();
          h_clone->SetDirectory(0);
          NormalizeHistogram(h_clone);

          styleKin_.StyleTH1(h_clone);
          h_clone->SetLineColor(m + 2);
          h_clone->SetLineWidth(2);

          double mean = h_clone->GetMean();
          double sigma = h_clone->GetStdDev();
          double x1 = mean - 3 * sigma;
          double x2 = mean + 3 * sigma;

          TLine* line1 = new TLine(x1, 0, x1, h_clone->GetMaximum() * 0.5);
          TLine* line2 = new TLine(x2, 0, x2, h_clone->GetMaximum() * 0.5);
          line1->SetLineColor(m + 2);
          line2->SetLineColor(m + 2);
          line1->SetLineStyle(2);  // Dashed
          line2->SetLineStyle(2);

          if (first) {
            h_clone->Draw("HIST");
            first = false;
          } else {
            h_clone->Draw("HIST SAME");
          }

          legend->AddEntry(h_clone, labels[m].c_str(), "l");
          std::ostringstream stats;
          stats << "#mu = " << std::fixed << std::setprecision(2) << mean << ", #sigma = " << std::fixed << std::setprecision(2) << sigma;
          legend->AddEntry((TObject*)0, stats.str().c_str(), "");
          line1->Draw("SAME");
          line2->Draw("SAME");
        }

        legend->Draw();
      }

      std::string outpath = outputDir + "/Exclusivity_" + cleanName + ".pdf";
      canvas->SaveAs(outpath.c_str());
      std::cout << "Saved detector-specific comparison to: " << outpath << "\n";
      delete canvas;
    }
  };

  void PlotDIS_BSA_Cross_Section_AndCorr_Comparison(double luminosity, double pol = 1.0, bool plotBSA = true, bool plotDVCSCross = false, bool plotPi0Corr = false,
                                                    bool meanKinVar = false) {
    if (plotters.empty()) {
      std::cerr << "No models loaded to compare.\n";
      return;
    }
    std::vector<std::vector<std::vector<std::vector<TH1D*>>>> allBSA;
    std::vector<std::vector<std::vector<std::vector<TH1D*>>>> allDVCSCross;
    std::vector<std::vector<std::vector<std::vector<TH1D*>>>> allPi0Corr;
    // job for chatgpt
    std::vector<std::vector<std::vector<std::vector<std::tuple<double, double, double>>>>> allBSAmeans;

    for (auto& p : plotters) {
      if (plotBSA) {
        auto h = p->ComputeBSA(fXbins, luminosity, pol);
        allBSA.push_back(std::move(h));
      }
      if (plotDVCSCross) {
        auto hists = p->ComputeDVCS_CrossSection(fXbins, luminosity);
        allDVCSCross.push_back(std::move(hists));
      }
      if (plotPi0Corr) {
        auto hcorr = p->ComputePi0Corr(fXbins);
        allPi0Corr.push_back(std::move(hcorr));
      }
      if (meanKinVar) {
        allBSAmeans.push_back(getMeanQ2xBt(fXbins, p));
      }
    }

    if (plotBSA) MakeTiledGridComparison("DIS_BSA", "A_{LU}", allBSA, &allBSAmeans, -0.65, 0.65, "png", true, true, false, meanKinVar);
    if (plotDVCSCross) MakeTiledGridComparison("DIS_Cross_Section", "d#sigma/d#phi [nb/deg]", allDVCSCross, &allBSAmeans, 0, 50000, "png", false, false, true, meanKinVar);
    if (plotPi0Corr) MakeTiledGridComparison("DIS_pi0Corr", "#eta^{#pi^{0}}", allPi0Corr, &allBSAmeans, 0.0, 1, "png", false, false, meanKinVar);
  }

  void MakeTiledGridComparison(const std::string& observableName, const std::string& yAxisTitle, const std::vector<std::vector<std::vector<std::vector<TH1D*>>>>& histograms,
                               const std::vector<std::vector<std::vector<std::vector<std::tuple<double, double, double>>>>>* meanValues, double yMin, double yMax,
                               const std::string& suffix = "png", bool fitSinusoid = false, bool setManualYrange = false, bool setLogY = false, bool showMeanKin = false) {
    if (histograms.empty() || histograms[0].empty() || histograms[0][0].empty() || histograms[0][0][0].empty()) {
      std::cerr << "No histograms to compare.\n";
      return;
    }
    const auto& q2_edges = fXbins.GetQ2Bins();
    const auto& t_edges = fXbins.GetTBins();
    const auto& xb_edges = fXbins.GetXBBins();

    const size_t n_q2 = q2_edges.size() - 1;
    const size_t n_t = t_edges.size() - 1;
    const size_t n_xb = xb_edges.size() - 1;

    const int rows = n_q2;
    const int cols = n_xb;

    bool Doplot = true;
    int first_perbin_xb = 0;
    bool first_perbin_q2 = true;

    for (size_t t_bin = 0; t_bin < n_t; ++t_bin) {
      TString cname = Form("DIS_BSA_t[%zu]", t_bin);
      TCanvas* c = new TCanvas(cname, cname, 2200, 1600);

      double canvasBorderX = 0.03;
      double canvasBorderY = 0.04;
      double gpad_margin_ratio = 0.2;

      double cellW = (1 - 2 * canvasBorderX) / cols, cellH = (1 - 2 * canvasBorderY) / rows;

      for (size_t q2_bin = 0; q2_bin < n_q2; ++q2_bin) {
        first_perbin_q2 = true;

        /// xbin loop
        for (size_t xb_bin = 0; xb_bin < n_xb; ++xb_bin) {
          int visualRow = rows - 1 - q2_bin;
          int pad = visualRow * cols + xb_bin + 1;
          c->cd();

          bool first = true;
          gStyle->SetCanvasPreferGL(true);

          TLegend* leg = new TLegend(0.35, 0.85, 0.85, 0.95);
          leg->SetBorderSize(0);
          leg->SetFillStyle(0);
          leg->SetTextSize(0.08);

          TLegend* legParams = new TLegend(0.35, 0.16, 0.85, 0.32);  // Bottom legend for a₁
          legParams->SetBorderSize(0);
          legParams->SetFillStyle(0);
          legParams->SetTextSize(0.08);

          TPad* thisPad = new TPad(Form("%zu_%zu", xb_bin, q2_bin), Form("%zu_%zu", xb_bin, q2_bin), cellW * xb_bin + canvasBorderX, cellH * (q2_bin) + canvasBorderY,
                                   cellW * (xb_bin + 1) + canvasBorderX, cellH * (q2_bin + 1) + canvasBorderY);
          double l = 0.00, r = 0.00, b = 0.00, t = 0.00;
          Doplot = false;

          for (size_t m = 0; m < histograms.size(); ++m) {
            // if(q2_bin == 2 && xb_bin == 2) continue; // save first per bin xb
            //  Pad margins
            /*
            double l = (first_perbin_q2) ? 0.2 : 0.00;
            double r = (!first_perbin_q2) ? 0.00 : 0.00;
            double b = (xb_bin == first_perbin_xb) ? 0.16 : 0.00;
            double t = (visualRow == 0) ? 0.000 : 0.00;
            */

            // const int idx = q2_bin * (n_t * n_xb) + t_bin * n_xb + xb_bin;

            TH1D* h = histograms[m][xb_bin][q2_bin][t_bin];

            styleBSA_.StyleTH1(h);

            auto [r, g, b] = modelShades[m % modelShades.size()];

            int colorIdx = 3000 + m * 20;  // Avoid low TColor indices

            if (!gROOT->GetColor(colorIdx)) {
              new TColor(colorIdx, r, g, b);
            }

            h->SetLineColor(colorIdx);
            h->SetMarkerColor(colorIdx);
            h->SetFillColorAlpha(colorIdx, 1.0);

            h->SetLineColor(colorIdx);
            h->SetMarkerColor(colorIdx);
            h->SetFillColorAlpha(colorIdx, 1.0);
            h->SetLineWidth(1);
            h->SetMarkerStyle(20);
            h->SetMarkerSize(1.0);
            h->SetStats(0);

            if (first) {
              l = (first_perbin_q2) ? (gpad_margin_ratio) / (1 + gpad_margin_ratio) : 0.00;
              r = (xb_bin == first_perbin_xb) ? (gpad_margin_ratio) / (1 + gpad_margin_ratio) : 0.00;
              b = (xb_bin == first_perbin_xb) ? (gpad_margin_ratio) / (1 + gpad_margin_ratio) : 0.00;
              t = (visualRow == 0) ? 0.000 : 0.00;

              l = (first_perbin_q2 && xb_bin == first_perbin_xb) ? (gpad_margin_ratio) / (1 + 2 * gpad_margin_ratio) : l;
              r = (xb_bin == first_perbin_xb && first_perbin_q2) ? (gpad_margin_ratio) / (1 + 2 * gpad_margin_ratio) : r;

              styleBSA_.StylePad(thisPad, l, r, b, t);
              thisPad->SetTicks(1, 0);
              thisPad->SetFillStyle(4000);
              // std::cout << "xb_bin: " << xb_bin << ", q2_bin: " << q2_bin << ", first_perbin_xb: " << first_perbin_xb << ", first_perbin_q2: " << first_perbin_q2 << std::endl;

              h->GetXaxis()->SetTitle((xb_bin == first_perbin_xb) ? "#phi [deg]" : "");
              h->GetYaxis()->SetTitle((first_perbin_q2) ? yAxisTitle.c_str() : "");
              h->GetXaxis()->SetLabelSize((xb_bin == first_perbin_xb) ? 0.085 : 0.0);
              h->GetXaxis()->SetTitleSize((xb_bin == first_perbin_xb) ? 0.095 : 0.0);
              h->GetYaxis()->SetLabelSize((first_perbin_q2) ? 0.085 : 0.0);
              h->GetYaxis()->SetTitleSize((first_perbin_q2) ? 0.1 : 0.0);
              if (xb_bin == first_perbin_xb && first_perbin_q2) {
                h->GetYaxis()->SetLabelSize(0.085 * (1 + gpad_margin_ratio) / (1 + 2 * gpad_margin_ratio));
                h->GetYaxis()->SetTitleSize(0.1 * (1 + gpad_margin_ratio) / (1 + 2 * gpad_margin_ratio));
              }
            }

            h->GetXaxis()->SetTitleOffset((xb_bin == first_perbin_xb) ? 0.82 : 0.0);
            h->GetYaxis()->SetTitleOffset((first_perbin_q2) ? 0.82 : 0.0);

            h->GetXaxis()->SetNdivisions(4, false);
            h->GetYaxis()->SetNdivisions(6, true);
            if (setManualYrange) h->GetYaxis()->SetRangeUser(yMin, yMax);
            h->GetXaxis()->SetRangeUser(0, 360);

            h->GetXaxis()->CenterTitle(true);
            h->GetYaxis()->CenterTitle(true);
            Doplot = !(!h || h->GetBinContent(5) == 0) || Doplot;
            if (!h || h->GetBinContent(5) == 0) {
              continue;
            }

            if (first) {
              if (xb_bin == first_perbin_xb && first_perbin_q2) {
                thisPad->SetPad(cellW * (xb_bin - gpad_margin_ratio) + canvasBorderX, cellH * (q2_bin - gpad_margin_ratio) + canvasBorderY,
                                cellW * (xb_bin + 1 + gpad_margin_ratio) + canvasBorderX, cellH * (q2_bin + 1) + canvasBorderY);
              } else if (xb_bin == first_perbin_xb && !first_perbin_q2) {
                h->GetXaxis()->ChangeLabel(1, -1, 0, -1, -1, -1, "");  // blank it out
                thisPad->SetPad(cellW * (xb_bin) + canvasBorderX, cellH * (q2_bin - gpad_margin_ratio) + canvasBorderY, cellW * (xb_bin + 1 + gpad_margin_ratio) + canvasBorderX,
                                cellH * (q2_bin + 1) + canvasBorderY);
              } else if (first_perbin_q2 && xb_bin != first_perbin_xb) {
                thisPad->SetPad(cellW * (xb_bin - gpad_margin_ratio) + canvasBorderX, cellH * (q2_bin) + canvasBorderY, cellW * (xb_bin + 1) + canvasBorderX,
                                cellH * (q2_bin + 1) + canvasBorderY);
              } else if (!first_perbin_q2 && xb_bin != first_perbin_xb) {
                thisPad->SetPad(cellW * (xb_bin) + canvasBorderX, cellH * (q2_bin) + canvasBorderY, cellW * (xb_bin + 1) + canvasBorderX, cellH * (q2_bin + 1) + canvasBorderY);
              }
            }

            if (first) thisPad->Draw();
            thisPad->cd();
            if (setLogY) thisPad->SetLogy();
            h->Draw(first ? "E1X0" : "E1X0 SAME");
            first = false;
            first_perbin_q2 = false;

            // Fit function and extract a₁
            if (fitSinusoid) {
              TF1* fitFunc = new TF1(Form("fit_%zu_%zu_%zu_%zu", m, t_bin, q2_bin, xb_bin), "[0] + ([1]*sin(x*TMath::DegToRad())) / (1 + [2]*cos(x*TMath::DegToRad()))", 0, 360);
              fitFunc->SetParameters(0.0, 0.2, 0.1);
              fitFunc->SetFillColorAlpha(colorIdx, 0.5);
              fitFunc->SetLineColorAlpha(colorIdx, 0.5);
              fitFunc->SetLineStyle(2);
              fitFunc->SetLineWidth(1);
              h->Fit(fitFunc, "Q0");
              fitFunc->Draw("SAME");

              double a1 = fitFunc->GetParameter(1);
              double a1e = fitFunc->GetParError(1);
              TString a1label = Form("a_{1} = %.2f #pm %.2f", a1, a1e);
              legParams->AddEntry(fitFunc, a1label, "l");
            }
            leg->AddEntry(h, labels[m].c_str(), "p");
            // auto [mean_xB, mean_Q2, mean_t] = meanValues[m][xb_bin][q2_bin][t_bin];
            if (showMeanKin) {
              auto [mean_xB, mean_Q2, mean_t] = (*meanValues)[m][xb_bin][q2_bin][t_bin];
              TString meanText = Form("<x_{B}> = %.2f, <Q^{2}> = %.2f, <t> = %.2f", mean_xB, mean_Q2, mean_t);
              TLatex* meanLatex = new TLatex(0.25, 0.78 - m * 0.10, meanText.Data());
              meanLatex->SetTextSize(0.06);
              meanLatex->SetNDC();
              meanLatex->SetTextFont(42);
              meanLatex->Draw();
            }
          }
          if (!Doplot) {
            std::cout << "No data for this bin combination, skipping...\n";
            continue;
          }
          /*
                    // Annotate bin ranges
                    double xB_low = xb_edges[xb_bin], xB_high = xb_edges[xb_bin + 1];
                    double Q2_low = q2_edges[q2_bin], Q2_high = q2_edges[q2_bin + 1];
                    TString labelText = Form("x_{B} #in [%.2f, %.2f], Q^{2} #in [%.1f, %.1f]", xB_low, xB_high, Q2_low, Q2_high);
                    TLatex* latex = new TLatex(0.25, 0.82, labelText.Data());
                    latex->SetTextSize(0.055);
                    latex->SetNDC();
                    latex->SetTextFont(42);
                    latex->Draw();
          */
          leg->Draw();
          if (fitSinusoid) legParams->Draw();

          thisPad->Modified();
          thisPad->Update();
          c->Modified();
          c->Update();
          if (xb_bin == first_perbin_xb) first_perbin_xb++;
        }
      }
      TString outfile = Form("%s/%s_t_%.2f-%.2f.%s", outputDir.c_str(), observableName.c_str(), t_edges[t_bin], t_edges[t_bin + 1], suffix.c_str());
      c->SaveAs(outfile);

      // std::cout << "Saved: " << outfile << '\n';

      delete c;
    }
  }

 private:
  BinManager fXbins;
  bool plotIndividual = false;

  DrawStyle style_;              // Default style
  DrawStyle styleKin_;           // Kin plot style
  DrawStyle styleDVCS_;          // DVCS plot style
  DrawStyle styleCrossSection_;  // Cross-section plot style
  DrawStyle styleBSA_;           // BSA plot style

  bool applyCorrection = false;
  THnSparseD* correctionHist = nullptr;

  std::unique_ptr<ROOT::RDF::RNode> rdf;
  std::string outputDir = ".";

  std::vector<std::unique_ptr<DISANAplotter>> plotters;
  std::vector<std::string> labels;

  std::vector<std::string> particleName = {"e", "p", "#gamma"};
  std::map<std::string, std::string> typeToParticle = {{"el", "electron"}, {"pro", "proton"}, {"pho", "#gamma"}};
  std::map<std::string, std::string> VarName = {{"p", "p (GeV/#it{c})"}, {"theta", "#theta (rad)"}, {"phi", "#phi(rad)"}};
};
#endif  // DISANA_COMPARER_H

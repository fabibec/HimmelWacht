#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include <array>
#include <fstream>
#include <sstream>

// Definiere Konstanten für die Modelleingabe
#define DEST_WIDTH 640
#define DEST_HEIGHT 640
#define CHANNELS 3
#define BATCH_SIZE 1
#define CONF_THRESHOLD 0.4

#define ORIG_WIDTH 640
#define ORIG_HEIGHT 480

using namespace std;
using namespace cv;

// Hilfsfunktion zum Schreiben von Debugging-Informationen in eine Datei
void debugLog(const std::string& message) {
    static std::ofstream log_file("debug_log.txt", std::ios::app);
    log_file << message << std::endl;
    log_file.flush(); // Sofortiges Schreiben in die Datei erzwingen
    std::cout << message << std::endl; // Auch auf der Konsole ausgeben
}

// Vorverarbeitung des Bildes für das Modell
cv::Mat preprocess(const cv::Mat& image) {
    try {
        debugLog("Starte Vorverarbeitung des Bildes");

        // Debug: Original-Bildabmessungen und -typ ausgeben
        debugLog("Original-Bild: " + std::to_string(image.cols) + "x" + std::to_string(image.rows) +
            ", Typ: " + std::to_string(image.type()));

        // Erstelle ein leeres Bild mit dem korrekten Seitenverhältnis und fülle es mit Schwarz
        float aspect_ratio = static_cast<float>(DEST_WIDTH) / DEST_HEIGHT;
        int new_width = image.cols;
        int new_height = static_cast<int>(new_width / aspect_ratio);

        if (new_height > image.rows) {
            new_height = image.rows;
            new_width = static_cast<int>(new_height * aspect_ratio);
        }

        // Bild auf das richtige Seitenverhältnis zuschneiden und dann auf die gewünschte Größe skalieren
        cv::Rect roi((image.cols - new_width) / 2, (image.rows - new_height) / 2, new_width, new_height);
        cv::Mat cropped = image(roi);

        cv::Mat resized_image;
        cv::resize(cropped, resized_image, Size(DEST_WIDTH, DEST_HEIGHT));

        // Debug: Größe nach Resize
        debugLog("Nach Resize: " + std::to_string(resized_image.cols) + "x" + std::to_string(resized_image.rows) +
            ", Typ: " + std::to_string(resized_image.type()));

        // Wichtig: Normalisierung auf 0-1 und BGR zu RGB konvertieren
        cv::Mat float_image;
        resized_image.convertTo(float_image, CV_32F, 1.0 / 255.0);

        // Umwandlung von BGR nach RGB, da YOLO üblicherweise RGB erwartet
        cv::Mat rgb_image;
        cv::cvtColor(float_image, rgb_image, cv::COLOR_BGR2RGB);

        // Debug: Typ nach Konvertierung
        debugLog("Nach Konvertierung: Typ: " + std::to_string(rgb_image.type()));

        debugLog("Vorverarbeitung des Bildes abgeschlossen");
        return rgb_image;
    }
    catch (const cv::Exception& e) {
        debugLog("Fehler bei der Bildvorverarbeitung: " + std::string(e.what()));
        throw;
    }
}

// OpenCV Mat zu ONNX-Tensor konvertieren
std::vector<float> matToVector(const cv::Mat& image) {
    try {
        debugLog("Starte Konvertierung von Mat zu Vector");

        // Überprüfe Bildformat
        if (image.type() != CV_32FC3) {
            debugLog("Falsches Bildformat: " + std::to_string(image.type()) + " statt CV_32FC3");
            throw std::runtime_error("Falsches Bildformat");
        }

        // Konvertiere HWC zu CHW (Höhe, Breite, Kanal zu Kanal, Höhe, Breite)
        std::vector<float> tensor_values(BATCH_SIZE * CHANNELS * DEST_HEIGHT * DEST_WIDTH);

        // Zähler für Debug-Zwecke
        int valid_values = 0;
        float min_val = 1.0f, max_val = 0.0f;

        for (int c = 0; c < CHANNELS; ++c) {
            for (int h = 0; h < DEST_HEIGHT; ++h) {
                for (int w = 0; w < DEST_WIDTH; ++w) {
                    // Index im Ziel-Tensor (NCHW-Format)
                    size_t tensor_idx = c * DEST_HEIGHT * DEST_WIDTH + h * DEST_WIDTH + w;

                    if (tensor_idx < tensor_values.size()) {
                        float pixel_value = image.at<cv::Vec3f>(h, w)[c];
                        tensor_values[tensor_idx] = pixel_value;

                        min_val = std::min(min_val, pixel_value);
                        max_val = std::max(max_val, pixel_value);
                        valid_values++;
                    }
                    else {
                        debugLog("Warnung: Index außerhalb des Bereichs: " + std::to_string(tensor_idx));
                    }
                }
            }
        }

        debugLog("Tensor-Werte: Min=" + std::to_string(min_val) + ", Max=" + std::to_string(max_val) +
            ", Gültige Werte: " + std::to_string(valid_values));
        debugLog("Mat zu Vector Konvertierung abgeschlossen");

        return tensor_values;
    }
    catch (const std::exception& e) {
        debugLog("Fehler bei der Mat-zu-Vector-Konvertierung: " + std::string(e.what()));
        throw;
    }
}

// Erstellt einen ONNX-Tensor aus einem Vektor von float-Werten
Ort::Value createTensorFromVector(const std::vector<float>& tensor_values, Ort::MemoryInfo& memory_info) {
    try {
        debugLog("Erstelle ONNX-Tensor aus Vector");

        // Definiere Tensor-Form (NCHW-Format)
        std::vector<int64_t> input_shape = { BATCH_SIZE, CHANNELS, DEST_HEIGHT, DEST_WIDTH };

        // Debug: Tensor-Form ausgeben
        std::stringstream shape_str;
        shape_str << "Tensor-Shape: [";
        for (size_t i = 0; i < input_shape.size(); ++i) {
            shape_str << input_shape[i];
            if (i < input_shape.size() - 1) shape_str << ", ";
        }
        shape_str << "]";
        debugLog(shape_str.str());

        // Überprüfe die Größe des Vektors
        size_t expected_size = BATCH_SIZE * CHANNELS * DEST_HEIGHT * DEST_WIDTH;
        if (tensor_values.size() != expected_size) {
            debugLog("Fehler: Tensor-Vektor hat falsche Größe. Ist: " +
                std::to_string(tensor_values.size()) + ", Erwartet: " +
                std::to_string(expected_size));
        }

        // Erstelle und gib den ONNX-Tensor zurück
        return Ort::Value::CreateTensor<float>(
            memory_info,
            const_cast<float*>(tensor_values.data()),
            tensor_values.size(),
            input_shape.data(),
            input_shape.size()
        );
    }
    catch (const std::exception& e) {
        debugLog("Fehler bei der Tensor-Erstellung: " + std::string(e.what()));
        throw;
    }
}

// Funktion zum Überprüfen der erwarteten Eingabeform des Modells
void checkModelInputShape(Ort::Session& session) {
    try {
        debugLog("Überprüfe erwartete Modelleingabe-Form");

        Ort::AllocatorWithDefaultOptions allocator;
        size_t num_input_nodes = session.GetInputCount();
        size_t num_output_nodes = session.GetOutputCount();

        debugLog("Anzahl der Eingabeknoten: " + std::to_string(num_input_nodes));
        debugLog("Anzahl der Ausgabeknoten: " + std::to_string(num_output_nodes));

        for (size_t i = 0; i < num_input_nodes; ++i) {
            // Name
            auto input_name = session.GetInputNameAllocated(i, allocator).get();
            debugLog("Input " + std::to_string(i) + " Name: " + input_name);

            // Type
            Ort::TypeInfo type_info = session.GetInputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            ONNXTensorElementDataType element_type = tensor_info.GetElementType();
            debugLog("Input " + std::to_string(i) + " Element Type: " + std::to_string(element_type));

            // Shape
            std::vector<int64_t> input_shape = tensor_info.GetShape();
            std::stringstream ss;
            ss << "Input " << i << " Shape: [";
            for (size_t j = 0; j < input_shape.size(); ++j) {
                ss << input_shape[j];
                if (j < input_shape.size() - 1) ss << ", ";
            }
            ss << "]";
            debugLog(ss.str());
        }

        // Auch die Ausgaben überprüfen
        for (size_t i = 0; i < num_output_nodes; ++i) {
            auto output_name = session.GetOutputNameAllocated(i, allocator).get();
            debugLog("Output " + std::to_string(i) + " Name: " + output_name);

            Ort::TypeInfo type_info = session.GetOutputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            ONNXTensorElementDataType element_type = tensor_info.GetElementType();
            debugLog("Output " + std::to_string(i) + " Element Type: " + std::to_string(element_type));

            std::vector<int64_t> output_shape = tensor_info.GetShape();
            std::stringstream ss;
            ss << "Output " << i << " Shape: [";
            for (size_t j = 0; j < output_shape.size(); ++j) {
                ss << output_shape[j];
                if (j < output_shape.size() - 1) ss << ", ";
            }
            ss << "]";
            debugLog(ss.str());
        }

        debugLog("Modellüberprüfung abgeschlossen");
    }
    catch (const Ort::Exception& e) {
        debugLog("Fehler bei der Modellüberprüfung: " + std::string(e.what()));
        throw;
    }
}

// Hauptfunktion
int main() {
    try {
        // Lösche vorherige Debug-Datei, wenn vorhanden
        std::ofstream clear_log("debug_log.txt", std::ios::trunc);
        clear_log.close();

        debugLog("Programm gestartet");

        // 1. SCHRITT - OpenCV-Video öffnen (ohne ONNX)
        debugLog("Versuche, Video zu öffnen");
        VideoCapture vid_capture("C:/Users/jendr/source/repos/image_seg_portable/utils/Test_video.mp4");

        if (!vid_capture.isOpened()) {
            debugLog("Fehler beim Öffnen des Videos");
            std::cerr << "Fehler beim Öffnen des Videos." << std::endl;
            return -1;
        }

        debugLog("Video erfolgreich geöffnet");
        double fps = vid_capture.get(5);
        double frame_count = vid_capture.get(7);
        debugLog("FPS: " + std::to_string(fps) + ", Frames: " + std::to_string(frame_count));

        // 2. SCHRITT - ONNX Runtime initialisieren
        debugLog("Initialisiere ONNX Runtime");
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "Yolov8n_custom");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

		//Cuda unterstützung aktivieren
		OrtCUDAProviderOptions cuda_options;
		cuda_options.device_id = 0;
		cuda_options.arena_extend_strategy = 0;
		cuda_options.gpu_mem_limit = 0;
		cuda_options.do_copy_in_default_stream = 1;
		session_options.AppendExecutionProvider_CUDA(cuda_options);


        // 3. SCHRITT - ONNX-Modell laden
        try {
            debugLog("Versuche, ONNX-Modell zu laden");
            std::string model_path = "C:/Users/jendr/source/repos/image_seg_portable/utils/yolov8n_custom.onnx";

            // Wandle den std::string in einen wchar_t* um
            std::wstring widestr = std::wstring(model_path.begin(), model_path.end());
            const wchar_t* widecstr = widestr.c_str();

            // Erstelle die Session
            Ort::Session session(env, widecstr, session_options);
            debugLog("ONNX-Modell erfolgreich geladen");

            // Überprüfe die erwartete Eingabeform des Modells
            checkModelInputShape(session);

            // 4. SCHRITT - Input/Output-Namen abrufen
            Ort::AllocatorWithDefaultOptions allocator;
            auto input_name = session.GetInputNameAllocated(0, allocator);
            auto output_name = session.GetOutputNameAllocated(0, allocator);

            debugLog("Input-Name: " + std::string(input_name.get()));
            debugLog("Output-Name: " + std::string(output_name.get()));

            // Erstelle C-style Strings für die API
            const char* input_names[] = { input_name.get() };
            const char* output_names[] = { output_name.get() };

            Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

            // 5. SCHRITT - Einzelne Frames verarbeiten
            int frame_count = 0;
            while (vid_capture.isOpened()) { // Beschränke auf 5 Frames für Debugging
                Mat frame;
                bool isSuccess = vid_capture.read(frame);

                if (!isSuccess) {
                    debugLog("Fehler beim Lesen des Frames oder Ende des Videos erreicht");
                    break;
                }

                frame_count++;
                debugLog("Frame " + std::to_string(frame_count) + " gelesen");

                // Zeige den Originalframe an (optional)
                cv::imshow("Original Frame", frame);

                try {
                    // 6. SCHRITT - Bild vorverarbeiten
                    cv::Mat preprocessed_frame = preprocess(frame);
                    debugLog("Vorverarbeitung für Frame " + std::to_string(frame_count) + " abgeschlossen");

                    // Zeige vorverarbeiteten Frame an (optional)
                    cv::Mat display_preprocessed;
                    preprocessed_frame.convertTo(display_preprocessed, CV_8U, 255.0);
                    cv::imshow("Preprocessed Frame", display_preprocessed);
                    int key = waitKey(1); // 10ms warten
                    if (key == 'q') break;

                    // 7. SCHRITT - Tensor erstellen - jetzt in zwei Schritten
                    std::vector<float> tensor_values = matToVector(preprocessed_frame);
                    Ort::Value input_tensor = createTensorFromVector(tensor_values, memory_info);
                    debugLog("Tensor für Frame " + std::to_string(frame_count) + " erstellt");

                    // 8. SCHRITT - Modellinferenz durchführen
                    debugLog("Starte Inferenz für Frame " + std::to_string(frame_count));
                    debugLog("Input Namen: " + std::string(input_names[0]));
                    debugLog("Output Namen: " + std::string(output_names[0]));

                    // Versuche die Inferenz mit erhöhtem Timeout
                    try {
                        Ort::RunOptions run_options;
                        debugLog("Führe Inferenz aus...");
                        auto output_tensors = session.Run(run_options, input_names, &input_tensor, 1, output_names, 1);
                        debugLog("Inferenz für Frame " + std::to_string(frame_count) + " erfolgreich abgeschlossen");

                        // Überprüfe die Ausgabe
                        auto tensor_info = output_tensors[0].GetTensorTypeAndShapeInfo();
                        auto output_shape = tensor_info.GetShape();
                        std::stringstream ss2;
                        ss2 << "Output Shape: [";
                        for (size_t j = 0; j < output_shape.size(); ++j) {
                            ss2 << output_shape[j];
                            if (j < output_shape.size() - 1) ss2 << ", ";
                        }
                        ss2 << "]";
                        debugLog(ss2.str());

						// Bounding Boxen extrahieren
						// Hier wird angenommen, dass die Ausgabe ein Vektor von Bounding Boxen ist
						// und dass die Bounding Boxen in der Form [x1, y1, x2, y2, confidence] vorliegen
						// Diese Struktur kann je nach Modell variieren
						const float* output_data = output_tensors[0].GetTensorMutableData<float>();
                        
						debugLog("Extrahiere Bounding Boxen aus der Modellausgabe");

                        std::stringstream ss;
						ss << "Rohwerte: [";
                        for (size_t i = 0; i < output_shape[1]; ++i) {
                            ss << output_data[i * 6 + 0] << ", " <<
                                output_data[i * 6 + 1] << ", " <<
                                output_data[i * 6 + 2] << ", " <<
                                output_data[i * 6 + 3] << ", " <<
                                output_data[i * 6 + 4];
                            if (i < output_shape[1] - 1) ss << "; ";
                        };
						ss << "]";
						debugLog(ss.str());


						std::vector<cv::Rect> boxes;
						std::vector<float> confidences;
                        for (size_t i = 0; i < output_shape[1]; ++i) {
                            // Rohwerte aus der Modellausgabe
                            float x1 = output_data[i * 6 + 0];
                            float y1 = output_data[i * 6 + 1];
                            float x2 = output_data[i * 6 + 2];
                            float y2 = output_data[i * 6 + 3];
                            float confidence = output_data[i * 6 + 4];

                            // Debug: Rohwerte ausgeben
                            debugLog("Rohwerte: x1=" + std::to_string(x1) +
                                ", y1=" + std::to_string(y1) +
                                ", x2=" + std::to_string(x2) +
                                ", y2=" + std::to_string(y2));

                            // Skalierung der Koordinaten auf die Bilddimensionen
                            float scale_x = static_cast<float>(frame.cols) / 640;
                            float scale_y = static_cast<float>(frame.rows) / 640;

                            x1 *= scale_x;
                            y1 *= scale_y;
                            x2 *= scale_x;
                            y2 *= scale_y;

                            // Validierung der Koordinaten (stellen Sie sicher, dass sie im Bildbereich liegen)
                            x1 = std::max(0.0f, std::min(x1, static_cast<float>(frame.cols - 1)));
                            y1 = std::max(0.0f, std::min(y1, static_cast<float>(frame.rows - 1)));
                            x2 = std::max(0.0f, std::min(x2, static_cast<float>(frame.cols - 1)));
                            y2 = std::max(0.0f, std::min(y2, static_cast<float>(frame.rows - 1)));

                            // Debug: Skalierte und validierte Werte ausgeben
                            debugLog("Skalierte Werte: x1=" + std::to_string(x1) +
                                ", y1=" + std::to_string(y1) +
                                ", x2=" + std::to_string(x2) +
                                ", y2=" + std::to_string(y2));

                            // Bounding Box und Konfidenz speichern, wenn der Schwellenwert überschritten wird
                            if (confidence > CONF_THRESHOLD) {
                                boxes.push_back(cv::Rect(cv::Point(static_cast<int>(x1), static_cast<int>(y1)),
                                    cv::Point(static_cast<int>(x2), static_cast<int>(y2))));
                                confidences.push_back(confidence);
                            }
                        }
						debugLog("Bounding Boxen extrahiert");
						// Zeichne die Bounding Boxen auf dem Originalbild
                        // Optional: Non-Maximum Suppression (NMS) anwenden
                        std::vector<int> indices;
                        cv::dnn::NMSBoxes(boxes, confidences, CONF_THRESHOLD, 0.4, indices);

                        // Zeichne die gefilterten Bounding Boxen auf das Bild
                        for (int idx : indices) {
                            cv::rectangle(frame, boxes[idx], cv::Scalar(0, 255, 0), 2);
                            std::string label = "Confidence: " + std::to_string(confidences[idx]);
                            cv::putText(frame, label, cv::Point(boxes[idx].x, boxes[idx].y - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
                        }
						// Zeige das Bild mit den Bounding Boxen an
						cv::imshow("Detected Objects", frame);
						int key = waitKey(1); // 10ms warten
						if (key == 'q') break;
						debugLog("Inferenz und Post-Processing für Frame " + std::to_string(frame_count) + " abgeschlossen");
						// Optional: Ausgabe der Bounding Boxen in die Konsole
						for (size_t i = 0; i < boxes.size(); ++i) {
							debugLog("Box " + std::to_string(i) + ": " +
								std::to_string(boxes[i].x) + ", " +
								std::to_string(boxes[i].y) + ", " +
								std::to_string(boxes[i].width) + ", " +
								std::to_string(boxes[i].height) + ", " +
								std::to_string(confidences[i]));
						}


                    }
                    catch (const Ort::Exception& e) {
                        debugLog("ONNX Inferenz-Fehler: " + std::string(e.what()));
                        // Zusätzliche Fehlerdetails anzeigen
                        debugLog("Fehlercode: " + std::to_string(e.GetOrtErrorCode()));
                    }
                }
                catch (const Ort::Exception& e) {
                    debugLog("ONNX-Fehler bei Frame " + std::to_string(frame_count) + ": " + std::string(e.what()));
                }
                catch (const std::exception& e) {
                    debugLog("Allgemeiner Fehler bei Frame " + std::to_string(frame_count) + ": " + std::string(e.what()));
                }

                debugLog("Frame " + std::to_string(frame_count) + " Verarbeitung abgeschlossen");
            }
        }
        catch (const Ort::Exception& e) {
            debugLog("ONNX-Fehler beim Laden des Modells: " + std::string(e.what()));
            std::cerr << "ONNX-Fehler: " << e.what() << std::endl;
        }

        debugLog("Video-Schleife beendet");
        vid_capture.release();
        cv::destroyAllWindows();
        debugLog("Programm erfolgreich beendet");

        // Am Ende warten, damit der Benutzer die Debug-Ausgabe lesen kann
        std::cout << "Programm beendet. Debug-Informationen wurden in debug_log.txt gespeichert." << std::endl;
        std::cout << "Drücken Sie eine Taste, um fortzufahren..." << std::endl;
        std::cin.get();

        return 0;
    }
    catch (const std::exception& e) {
        debugLog("Unbehandelter Ausnahmefehler: " + std::string(e.what()));
        std::cerr << "Unbehandelter Fehler: " << e.what() << std::endl;
        std::cout << "Drücken Sie eine Taste, um fortzufahren..." << std::endl;
        std::cin.get();
        return -1;
    }
}